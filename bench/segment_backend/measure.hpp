// Copyright 2025 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_SEGMENT_BACKEND_MEASURE_HPP
#define METALL_BENCH_SEGMENT_BACKEND_MEASURE_HPP

// Measurement helpers shared by the segment backend arms: timing, latency
// percentiles, resident memory, disk usage with hard links counted once,
// the file system behind a path, a page-granular record of what the
// workload wrote, and a small JSON writer.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>
#ifdef __linux__
#include <sys/statfs.h>
#endif

namespace metall::bench::segment_backend {

using clock_type = std::chrono::steady_clock;

inline double seconds_since(const clock_type::time_point start) {
  return std::chrono::duration<double>(clock_type::now() - start).count();
}

/// \brief Latency samples in nanoseconds, reduced to percentiles on demand.
class latency_series {
 public:
  explicit latency_series(const std::size_t reserve = 0) {
    m_samples.reserve(reserve);
  }

  void add(const double ns) { m_samples.push_back(ns); }

  [[nodiscard]] std::size_t count() const { return m_samples.size(); }

  /// \brief The quantile of the samples. Sorts on first use.
  [[nodiscard]] double quantile(const double q) {
    if (m_samples.empty()) return 0.0;
    if (!m_sorted) {
      std::sort(m_samples.begin(), m_samples.end());
      m_sorted = true;
    }
    const auto index = static_cast<std::size_t>(
        q * static_cast<double>(m_samples.size() - 1));
    return m_samples[index];
  }

  [[nodiscard]] double mean() const {
    if (m_samples.empty()) return 0.0;
    double sum = 0.0;
    for (const double sample : m_samples) sum += sample;
    return sum / static_cast<double>(m_samples.size());
  }

 private:
  std::vector<double> m_samples{};
  bool m_sorted{false};
};

/// \brief A value out of /proc/self/status, in bytes. 0 where it is absent.
inline std::uint64_t proc_status_bytes(const char *const key) {
  std::ifstream status("/proc/self/status");
  if (!status) return 0;
  std::string line;
  const std::size_t key_length = std::strlen(key);
  while (std::getline(status, line)) {
    if (line.compare(0, key_length, key) != 0) continue;
    std::uint64_t kib = 0;
    if (std::sscanf(line.c_str() + key_length, ": %lu", &kib) != 1) return 0;
    return kib * 1024;
  }
  return 0;
}

/// \brief Resident set size now.
inline std::uint64_t rss_bytes() { return proc_status_bytes("VmRSS"); }

/// \brief The high water mark of the resident set size. It covers the whole
/// process life, so it is only a phase number for the first phase that runs.
inline std::uint64_t rss_peak_bytes() { return proc_status_bytes("VmHWM"); }

/// \brief Bytes and files under a set of paths. Hard-linked files are counted
/// once, which is what makes a snapshot series of a block store comparable to
/// a series of copies.
class disk_usage {
 public:
  /// \brief Adds one directory tree. Files already seen through another link
  /// or another tree add nothing.
  void add_tree(const std::filesystem::path &root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return;
    add_entry(root);
    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         it != end && !ec; it.increment(ec)) {
      add_entry(it->path());
    }
  }

  [[nodiscard]] std::uint64_t apparent_bytes() const { return m_apparent; }
  /// \brief Blocks the file system allocated, which is what a sparse file or a
  /// shared block file costs in reality.
  [[nodiscard]] std::uint64_t allocated_bytes() const { return m_allocated; }
  [[nodiscard]] std::uint64_t files() const { return m_files; }

 private:
  void add_entry(const std::filesystem::path &path) {
    struct ::stat info{};
    if (::lstat(path.c_str(), &info) != 0) return;
    if (!S_ISREG(info.st_mode)) return;
    if (!m_seen.emplace(info.st_dev, info.st_ino).second) return;
    m_apparent += static_cast<std::uint64_t>(info.st_size);
    m_allocated += static_cast<std::uint64_t>(info.st_blocks) * 512;
    ++m_files;
  }

  std::set<std::pair<std::uint64_t, std::uint64_t>> m_seen{};
  std::uint64_t m_apparent{0};
  std::uint64_t m_allocated{0};
  std::uint64_t m_files{0};
};

/// \brief Name of the file system holding a path. A datastore on tmpfs makes
/// every durability barrier a no-op, so the name belongs in the results.
inline std::string filesystem_name(const std::filesystem::path &path) {
#ifdef __linux__
  struct ::statfs info{};
  if (::statfs(path.c_str(), &info) != 0) return "unknown";
  switch (static_cast<std::uint64_t>(info.f_type)) {
    case 0xef53:
      return "ext4";
    case 0x01021994:
      return "tmpfs";
    case 0x794c7630:
      return "overlayfs";
    case 0x58465342:
      return "xfs";
    case 0x9123683e:
      return "btrfs";
    case 0x6a656a63:
      return "virtiofs";
    case 0x65735546:
      return "fuse";
    default: {
      std::ostringstream out;
      out << "0x" << std::hex << static_cast<std::uint64_t>(info.f_type);
      return out.str();
    }
  }
#else
  (void)path;
  return "unknown";
#endif
}

/// \brief Which pages of the segment the workload wrote to, one bit per page.
///
/// It counts the stores the benchmark makes itself. The allocator's own
/// metadata writes are not counted, so the page count is a lower bound on the
/// dirty set, and a write amplification computed against it is an upper bound.
/// Each writing thread keeps its own instance and they are merged afterwards,
/// so the hot path needs no synchronization.
class page_map {
 public:
  page_map() = default;

  page_map(const void *const segment_base, const std::uint64_t capacity,
           const std::uint64_t page_size)
      : m_base(reinterpret_cast<std::uintptr_t>(segment_base)),
        m_page_size(page_size),
        m_bits((capacity / page_size + 64) / 64, 0) {}

  void touch(const void *const address, const std::size_t length) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (value < m_base) {
      ++m_out_of_range;
      return;
    }
    const std::uint64_t first = (value - m_base) / m_page_size;
    const std::uint64_t last = (value - m_base + length - 1) / m_page_size;
    for (std::uint64_t page = first; page <= last; ++page) {
      if (page / 64 >= m_bits.size()) {
        ++m_out_of_range;
        return;
      }
      m_bits[page / 64] |= std::uint64_t{1} << (page % 64);
    }
  }

  void merge(const page_map &other) {
    if (m_bits.size() < other.m_bits.size()) m_bits.resize(other.m_bits.size());
    for (std::size_t word = 0; word < other.m_bits.size(); ++word) {
      m_bits[word] |= other.m_bits[word];
    }
    m_out_of_range += other.m_out_of_range;
  }

  void clear() {
    std::fill(m_bits.begin(), m_bits.end(), std::uint64_t{0});
    m_out_of_range = 0;
  }

  [[nodiscard]] std::uint64_t pages() const {
    std::uint64_t count = 0;
    for (const std::uint64_t word : m_bits) {
      count += static_cast<std::uint64_t>(__builtin_popcountll(word));
    }
    return count;
  }

  /// \brief Distinct blocks of block_size the touched pages fall into, which
  /// is how many blocks a block-granular backend has to write.
  [[nodiscard]] std::uint64_t blocks(const std::uint64_t block_size) const {
    if (block_size == 0) return 0;
    const std::uint64_t pages_per_block = block_size / m_page_size;
    std::uint64_t count = 0;
    std::uint64_t current = UINT64_MAX;
    for (std::size_t word = 0; word < m_bits.size(); ++word) {
      if (m_bits[word] == 0) continue;
      for (std::uint64_t bit = 0; bit < 64; ++bit) {
        if ((m_bits[word] & (std::uint64_t{1} << bit)) == 0) continue;
        const std::uint64_t block =
            (static_cast<std::uint64_t>(word) * 64 + bit) / pages_per_block;
        if (block != current) {
          current = block;
          ++count;
        }
      }
    }
    return count;
  }

  [[nodiscard]] std::uint64_t out_of_range() const { return m_out_of_range; }

 private:
  std::uintptr_t m_base{0};
  std::uint64_t m_page_size{4096};
  std::vector<std::uint64_t> m_bits{};
  std::uint64_t m_out_of_range{0};
};

/// \brief Writes JSON to a stream. Objects and arrays are opened and closed by
/// hand; the writer only tracks whether a separator is due.
class json_writer {
 public:
  explicit json_writer(std::ostream &out) : m_out(out) {}

  void begin_object(const char *const key = nullptr) {
    separate();
    if (key != nullptr) m_out << '"' << key << "\": ";
    m_out << '{';
    m_first.push_back(true);
  }

  void end_object() {
    m_first.pop_back();
    m_out << '}';
  }

  void begin_array(const char *const key = nullptr) {
    separate();
    if (key != nullptr) m_out << '"' << key << "\": ";
    m_out << '[';
    m_first.push_back(true);
  }

  void end_array() {
    m_first.pop_back();
    m_out << ']';
  }

  void key_value(const char *const key, const std::string &value) {
    separate();
    m_out << '"' << key << "\": \"" << value << '"';
  }

  void key_value(const char *const key, const char *const value) {
    key_value(key, std::string(value));
  }

  void key_value(const char *const key, const bool value) {
    separate();
    m_out << '"' << key << "\": " << (value ? "true" : "false");
  }

  void key_value(const char *const key, const std::uint64_t value) {
    separate();
    m_out << '"' << key << "\": " << value;
  }

  void key_value(const char *const key, const double value) {
    separate();
    m_out << '"' << key << "\": " << value;
  }

 private:
  void separate() {
    if (m_first.empty()) return;
    if (!m_first.back()) m_out << ", ";
    m_first.back() = false;
  }

  std::ostream &m_out;
  std::vector<bool> m_first{};
};

}  // namespace metall::bench::segment_backend

#endif  // METALL_BENCH_SEGMENT_BACKEND_MEASURE_HPP
