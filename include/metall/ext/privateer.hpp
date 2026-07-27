// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_EXT_PRIVATEER_HPP
#define METALL_EXT_PRIVATEER_HPP

// Segment storage backed by a Privateer datastore.
//
// The segment is one contiguous VM reservation owned by the engine:
//
//   [ segment header | block 0 | block 1 | ... ]
//
// The header is volatile anonymous memory, so metall constructs its
// segment_header there on every create and open. Each block of the extended
// size maps one content-addressed file read-only, or anonymous zeros where
// the datastore holds nothing yet. The first write into a block faults, the
// engine makes that block writable and counts it dirty, and the retried
// store lands; later writes to the same block are native. sync() writes the
// dirty blocks back under new names and replaces the recipe in one atomic
// step, so a checkpoint costs the dirty data instead of the datastore size,
// on every file system.
//
// Divergences from the default backend:
// - The capacity is fixed when the datastore is created. A larger capacity
//   request at open cannot be honoured, and is reported and ignored.
// - free_region() reclaims whole blocks. A range that covers no whole block
//   frees nothing; the allocator reuses the space either way.
// - A failed extend() leaves the segment as it was, so the storage stays
//   usable instead of turning broken.
// - sync() is the only path that makes data durable. The destructor mirrors
//   the default backend and syncs, but metall's close() is what drives the
//   sequence and reports failures.
//
// Datastore-wide settings (block size, hash algorithm, background
// write-back, memory budgets) have no place in metall's manager API. An
// application sets them through options() before it constructs a manager.

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <privateer/error.hpp>
#include <privateer/region.hpp>
#include <privateer/vm.hpp>

// The compile-time configuration comes first: the kernel headers below read
// its macros while they are parsed.
#include <metall/defs.hpp>
#include <metall/basic_manager.hpp>
#include <metall/logger.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/storage.hpp>

namespace metall {

class privateer_segment_storage;

/// \brief Metall manager that keeps its segment in a Privateer datastore.
using manager_privateer =
    basic_manager<kernel::storage, privateer_segment_storage>;

#ifdef METALL_USE_PRIVATEER
using manager = manager_privateer;
#endif

class privateer_segment_storage {
 public:
  using path_type = kernel::storage::path_type;
  using segment_header_type = kernel::segment_header;

  /// \brief Engine options used by the next create() or open() in this
  /// process. The engine defaults are 8 MiB blocks, xxh3-128 block names, no
  /// background write-back and no memory budget;
  /// METALL_PRIVATEER_BLOCK_SIZE overrides the block size at compile time.
  /// The header size is set by this class and cannot be configured.
  static privateer::region_options &options() noexcept {
    static privateer::region_options opts = priv_initial_options();
    return opts;
  }

  privateer_segment_storage() : m_page_size(privateer::page_size()) {}

  ~privateer_segment_storage() {
    if (!is_open()) {
      return;
    }
    bool succeeded = read_only() || sync(true);
    succeeded &= release();
    if (!succeeded) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to destruct");
    }
  }

  privateer_segment_storage(const privateer_segment_storage &) = delete;
  privateer_segment_storage &operator=(const privateer_segment_storage &) =
      delete;

  privateer_segment_storage(privateer_segment_storage &&other) noexcept
      : m_page_size(other.m_page_size),
        m_region(std::move(other.m_region)),
        m_read_only(other.m_read_only),
        m_broken(other.m_broken) {
    other.priv_set_broken_status();
  }

  privateer_segment_storage &operator=(
      privateer_segment_storage &&other) noexcept {
    m_page_size = other.m_page_size;
    m_region = std::move(other.m_region);
    m_read_only = other.m_read_only;
    m_broken = other.m_broken;
    other.priv_set_broken_status();
    return (*this);
  }

  /// \brief Copies a datastore's segment to another location. The source is
  /// never modified: block files are hard-linked where the file system
  /// allows it, and copied where it does not.
  /// \param source_path A path to a source datastore.
  /// \param destination_path A destination path.
  /// \param clone Ignored. Sharing block files does not need reflink
  /// support.
  /// \param max_num_threads Ignored. The engine picks its own parallelism.
  /// \return Return true if success; otherwise, false.
  static bool copy(const path_type &source_path,
                   const path_type &destination_path,
                   [[maybe_unused]] const bool clone,
                   [[maybe_unused]] const int max_num_threads) {
    auto copied = privateer::region::copy(priv_segment_path(source_path),
                                         priv_segment_path(destination_path));
    if (!copied) {
      priv_log_error("Failed to copy the segment", copied.error());
      return false;
    }
    return true;
  }

  /// \brief Creates a new datastore.
  /// Calling this function fails if this class already manages an opened
  /// segment.
  /// \param base_path A base directory path to create a datastore.
  /// \param capacity A segment capacity to reserve. It is fixed for the
  /// lifetime of the datastore.
  /// \return Return true if success; otherwise, false.
  bool create(const path_type &base_path, const std::size_t capacity) {
    if (!check_sanity()) return false;
    if (is_open()) return false;  // Cannot open multiple segments at once.

    auto opts = options();
    opts.header_size = sizeof(segment_header_type);
    auto region = privateer::region::create(priv_segment_path(base_path),
                                           capacity, opts);
    if (!region) {
      priv_log_error("Failed to create the segment", region.error());
      // No mapping was published, so only this instance is unusable.
      priv_set_broken_status();
      return false;
    }
    m_region.emplace(std::move(region.value()));
    m_read_only = false;

    // metall assumes that a segment always holds at least one block.
    if (!extend(m_region->block_size())) {
      priv_set_broken_status();
      return false;
    }

    priv_construct_segment_header();
    return true;
  }

  /// \brief Opens an existing datastore.
  /// Calling this function fails if this class already manages an opened
  /// segment.
  /// \param base_path A base directory path of an existing datastore.
  /// \param capacity A segment capacity request. The capacity of a datastore
  /// is fixed when it is created, so a larger request cannot be honoured.
  /// \param read_only If true, this segment is read only.
  /// \return Return true if success; otherwise, false.
  bool open(const path_type &base_path, const std::size_t capacity,
            const bool read_only) {
    if (!check_sanity()) return false;
    if (is_open()) return false;  // Cannot open multiple segments at once.

    auto opts = options();
    opts.header_size = sizeof(segment_header_type);
    const auto segment_path = priv_segment_path(base_path);
    auto region = read_only ? privateer::region::open_read_only(segment_path,
                                                               opts)
                            : privateer::region::open(segment_path, opts);
    if (!region) {
      priv_log_error("Failed to open the segment", region.error());
      priv_set_broken_status();
      return false;
    }
    m_region.emplace(std::move(region.value()));
    m_read_only = read_only;

    if (!read_only && capacity > m_region->capacity()) {
      std::stringstream ss;
      ss << "Capacity request of " << capacity
         << " bytes is ignored; this datastore holds up to "
         << m_region->capacity() << " bytes, fixed when it was created";
      logger::out(logger::level::verbose, __FILE__, __LINE__,
                  ss.str().c_str());
    }

    priv_construct_segment_header();
    return true;
  }

  /// \brief Extends the currently opened segment if necessary.
  /// \param request_size A segment size to extend to. It is rounded up to
  /// whole blocks.
  /// \return Returns true if the segment is extended to or already larger
  /// than the requested size. Returns false on failure; the segment is then
  /// unchanged and still usable.
  bool extend(const std::size_t request_size) {
    if (!is_open()) return false;
    if (m_read_only) return false;
    if (request_size <= size()) return true;  // Already large enough.

    auto extended = m_region->extend(request_size);
    if (!extended) {
      priv_log_error("Failed to extend the segment", extended.error());
      return false;
    }
    return true;
  }

  /// \brief Releases the segment. Data that sync() did not write is lost.
  /// \return Return true if success; otherwise, false. A false return means
  /// the datastore may be incomplete on disk, so metall withholds the
  /// properly-closed mark.
  bool release() {
    if (!is_open()) return false;

    const bool succeeded = m_region->check_sanity();
    if (!succeeded) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "The segment recorded a failure while it was open");
    }
    // Closing the region joins its background work, unregisters the write
    // barrier and releases the reservation.
    m_region.reset();
    return succeeded;
  }

  /// \brief Writes every dirty block back to the datastore and replaces the
  /// recipe.
  /// \param sync If false, the new state is written but not flushed to the
  /// device, so a crash can lose it.
  /// \return Return true if success; otherwise, false.
  bool sync(const bool sync) {
    if (!is_open()) return false;
    if (m_read_only) return true;

    auto committed = m_region->commit(sync);
    if (!committed) {
      priv_log_error("Failed to synchronize the segment", committed.error());
      return false;
    }
    return true;
  }

  /// \brief Tries to free the specified region in DRAM and file(s). Only
  /// whole blocks are freed; their files are reclaimed by the next
  /// synchronous sync().
  /// \param offset An offset to the region from the beginning of the
  /// segment.
  /// \param nbytes The size of the region.
  bool free_region(const std::ptrdiff_t offset, const std::size_t nbytes) {
    if (!is_open() || m_read_only) return false;

    auto freed = m_region->free_region(static_cast<std::uint64_t>(offset),
                                      static_cast<std::uint64_t>(nbytes));
    if (!freed) {
      priv_log_error("Failed to free a region", freed.error());
      return false;
    }
    return true;
  }

  /// \brief Stages a snapshot of the segment under the given base path. The
  /// caller publishes it.
  /// \param snapshot_path A path to a snapshot.
  /// \param clone Ignored. Sharing block files does not need reflink
  /// support.
  /// \param max_num_threads Ignored. The engine picks its own parallelism.
  /// \return Return true if success; otherwise, false.
  bool snapshot(const path_type &snapshot_path,
                [[maybe_unused]] const bool clone,
                [[maybe_unused]] const int max_num_threads) {
    if (!is_open()) return false;

    auto staged = m_region->snapshot_to(priv_segment_path(snapshot_path));
    if (!staged) {
      priv_log_error("Failed to snapshot the segment", staged.error());
      return false;
    }
    return true;
  }

  /// \brief Returns the address of the segment.
  /// \return The address of the segment.
  void *get_segment() const {
    return is_open() ? m_region->segment() : nullptr;
  }

  /// \brief Returns a reference to the segment header.
  /// \return A reference to the segment header.
  segment_header_type &get_segment_header() {
    return *static_cast<segment_header_type *>(m_region->segment_header());
  }

  /// \brief Returns a reference to the segment header.
  /// \return A reference to the segment header.
  const segment_header_type &get_segment_header() const {
    return *static_cast<const segment_header_type *>(
        m_region->segment_header());
  }

  /// \brief Returns the current segment size.
  /// \return The current segment size.
  std::size_t size() const { return is_open() ? m_region->size() : 0; }

  /// \brief Returns the underlying page size.
  /// \return The page size of the system.
  std::size_t page_size() const { return m_page_size; }

  /// \brief Checks if the segment is read only.
  /// \return Returns true if the segment is read only; otherwise, returns
  /// false.
  bool read_only() const { return m_read_only; }

  /// \brief Checks if there is a segment already open.
  /// \return Returns true if there is a segment already open.
  bool is_open() const { return m_region.has_value(); }

  /// \brief Checks the sanity of the instance.
  /// \return Returns true if there is no issue; otherwise, returns false.
  /// If false is returned, the instance of this class cannot be used
  /// anymore.
  bool check_sanity() const {
    if (m_broken) return false;
    return is_open() ? m_region->check_sanity() : true;
  }

  /// \brief Returns the write-back and backpressure counters of the open
  /// segment. All zero when no segment is open.
  privateer::region_statistics statistics() const {
    return is_open() ? m_region->statistics() : privateer::region_statistics{};
  }

 private:
  static constexpr const char *k_dir_name = "segment";

  static privateer::region_options priv_initial_options() {
    privateer::region_options opts;
#ifdef METALL_PRIVATEER_BLOCK_SIZE
    opts.block_size = METALL_PRIVATEER_BLOCK_SIZE;
#endif
    return opts;
  }

  static path_type priv_segment_path(const path_type &base_path) {
    return kernel::storage::get_path(base_path, k_dir_name);
  }

  static void priv_log_error(const char *what, const privateer::error &error) {
    const std::string message =
        std::string(what) + ": " + privateer::to_string(error);
    logger::out(logger::level::error, __FILE__, __LINE__, message.c_str());
  }

  void priv_set_broken_status() {
    m_region.reset();
    m_broken = true;
    // m_read_only must not be modified here.
  }

  // The header is volatile anonymous memory, zeroed by every create and
  // open, so the segment header object is constructed each time.
  void priv_construct_segment_header() {
    new (m_region->segment_header()) segment_header_type();
  }

  std::size_t m_page_size{0};
  std::optional<privateer::region> m_region{};
  bool m_read_only{false};
  bool m_broken{false};
};

}  // namespace metall

#endif  // METALL_EXT_PRIVATEER_HPP
