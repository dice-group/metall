// Copyright 2025 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// Compares metall's segment backends on one workload: the default backend
// against the privateer backend. Both live in this binary, so one run holds
// the machine, the file system and the workload fixed across the arms it
// compares. The translation unit does not define METALL_USE_PRIVATEER, so
// metall::manager stays the default backend and manager_privateer is the
// other arm.
//
// Phases, per arm:
//   load       build the node graph in a fresh datastore, then one checkpoint
//   read       root to leaf chases with the datastore still open
//   reopen     close, open again, chase again: open cost and first-touch reads
//   mixed      readers chasing while a writer path-copies and a collector
//              frees, with a checkpoint every so many updates
//   retention  a series of snapshots, and deleting the oldest once the series
//              is longer than the retention window
//
// Numbers are written as JSON. Run it with the datastore on a real disk: on
// tmpfs every durability barrier is a no-op and the checkpoint numbers mean
// nothing.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include <metall/metall.hpp>

#ifdef METALL_BENCH_WITH_PRIVATEER
#include <metall/ext/privateer.hpp>
#endif

#include "measure.hpp"
#include "node_graph.hpp"

namespace {

using namespace metall::bench::segment_backend;

struct options {
  std::filesystem::path root = "./segment_backend_bench";
  bool run_default = true;
  bool run_privateer = true;
  graph_config graph{};
  std::uint64_t capacity = 8ULL << 30;
  std::uint64_t chase_ops = 400000;
  std::uint32_t readers = 4;
  std::uint64_t updates = 100000;
  std::uint64_t checkpoint_every = 2000;
  std::uint32_t snapshots = 8;
  std::uint32_t retain = 4;
  std::uint64_t block_size = 0;  // 0 keeps the engine default
  std::string cleaner = "off";
  // Cadence and batch size of the background sweep; 0 keeps the engine
  // default. They set the write-back rate, so a mode alone says little: the
  // engine default of 8 slots every second is 16 MiB/s at a 2 MiB block, far
  // below what a bulk load dirties.
  std::uint64_t cleaner_interval_ms = 0;
  std::uint64_t cleaner_batch_slots = 0;
  std::uint64_t dirty_soft = 0;
  std::uint64_t dirty_low = 0;
  std::uint64_t dirty_hard = 0;
  std::uint64_t resident_soft = 0;
  std::uint64_t resident_low = 0;
  std::uint64_t sweep_interval_ms = 0;  // 0 keeps the engine default
  // Cadence of the resident-size sampler over the mixed phase; 0 samples
  // nothing.
  std::uint64_t rss_sample_ms = 20;
  std::string json_path{};
};

/// \brief What a backend can be asked to do beyond metall's own API. The
/// default backend answers nothing; the privateer arm specializes it.
template <typename Manager>
struct backend_traits {
  static constexpr const char *name = "default";
  static void configure(const options &) {}
  static void arm_thread() {}
  static std::uint64_t block_size(const options &) { return 0; }
};

/// \brief The counters a backend reports about its own write-out work.
struct backend_statistics {
  bool available = false;
  std::uint64_t slots_hashed = 0;
  std::uint64_t slots_skipped = 0;
  std::uint64_t slots_deduped = 0;
  std::uint64_t slots_written = 0;
  std::uint64_t slots_cleaned = 0;
  std::uint64_t slots_redirtied = 0;
  std::uint64_t writer_stalls = 0;
};

template <typename Manager>
backend_statistics read_statistics(const Manager &manager);

template <>
backend_statistics read_statistics<metall::manager>(const metall::manager &) {
  return {};
}

#ifdef METALL_BENCH_WITH_PRIVATEER
template <>
struct backend_traits<metall::manager_privateer> {
  static constexpr const char *name = "privateer";

  static void configure(const options &opt) {
    auto &engine = metall::privateer_segment_storage::options();
    if (opt.block_size != 0) engine.block_size = opt.block_size;
    engine.cleaner.mode = cleaner_mode_of(opt.cleaner);
    if (opt.cleaner_interval_ms != 0) {
      engine.cleaner.interval =
          std::chrono::milliseconds{opt.cleaner_interval_ms};
    }
    if (opt.cleaner_batch_slots != 0) {
      engine.cleaner.batch_slots =
          static_cast<std::size_t>(opt.cleaner_batch_slots);
    }
    engine.governor.dirty_soft = opt.dirty_soft;
    engine.governor.dirty_low = opt.dirty_low;
    engine.governor.dirty_hard = opt.dirty_hard;
    engine.governor.resident_soft = opt.resident_soft;
    engine.governor.resident_low = opt.resident_low;
    if (opt.sweep_interval_ms != 0) {
      engine.governor.sweep_interval =
          std::chrono::milliseconds{opt.sweep_interval_ms};
    }
  }

  static void arm_thread() {
    metall::privateer_segment_storage::arm_calling_thread();
  }

  static std::uint64_t block_size(const options &) {
    return metall::privateer_segment_storage::options().block_size.value_or(
        privateer::default_block_size);
  }

 private:
  static privateer::cleaner_mode cleaner_mode_of(const std::string &name) {
    if (name == "non_durable") return privateer::cleaner_mode::non_durable;
    if (name == "eager_durable") return privateer::cleaner_mode::eager_durable;
    return privateer::cleaner_mode::off;
  }
};

template <>
backend_statistics read_statistics<metall::manager_privateer>(
    const metall::manager_privateer &manager) {
  const auto *const storage = manager.get_segment_storage();
  if (storage == nullptr) return {};
  const privateer::region_statistics stats = storage->statistics();
  return {.available = true,
          .slots_hashed = stats.slots_hashed,
          .slots_skipped = stats.slots_skipped,
          .slots_deduped = stats.slots_deduped,
          .slots_written = stats.slots_written,
          .slots_cleaned = stats.slots_cleaned,
          .slots_redirtied = stats.slots_redirtied,
          .writer_stalls = stats.writer_stalls};
}
#endif

void write_statistics(json_writer &json, const backend_statistics &stats) {
  json.begin_object("engine_counters");
  json.key_value("available", stats.available);
  if (stats.available) {
    json.key_value("slots_hashed", stats.slots_hashed);
    json.key_value("slots_skipped", stats.slots_skipped);
    json.key_value("slots_deduped", stats.slots_deduped);
    json.key_value("slots_written", stats.slots_written);
    json.key_value("slots_cleaned", stats.slots_cleaned);
    json.key_value("slots_redirtied", stats.slots_redirtied);
    json.key_value("writer_stalls", stats.writer_stalls);
  }
  json.end_object();
}

/// \brief Bytes and files of the datastore, and of the datastore plus every
/// snapshot with hard-linked files counted once.
struct store_size {
  std::uint64_t datastore_allocated = 0;
  std::uint64_t datastore_apparent = 0;
  std::uint64_t datastore_files = 0;
};

store_size measure_store(const std::filesystem::path &datastore) {
  disk_usage usage;
  usage.add_tree(datastore);
  return {usage.allocated_bytes(), usage.apparent_bytes(), usage.files()};
}

void write_store_size(json_writer &json, const char *const key,
                      const store_size &size) {
  json.begin_object(key);
  json.key_value("allocated_bytes", size.datastore_allocated);
  json.key_value("apparent_bytes", size.datastore_apparent);
  json.key_value("files", size.datastore_files);
  json.end_object();
}

struct read_result {
  std::uint64_t ops = 0;
  double seconds = 0.0;
  double p50_ns = 0.0;
  double p99_ns = 0.0;
  double max_ns = 0.0;
  std::uint64_t bad_nodes = 0;
};

/// \brief Runs chases from reader threads. Readers only read, so they need no
/// alternate signal stack and no arming.
template <typename Manager>
read_result run_readers(node_graph<Manager> &graph, const std::uint64_t ops,
                        const std::uint32_t threads, const std::uint64_t seed,
                        const bool sample_latency) {
  const std::uint64_t per_thread = threads == 0 ? ops : ops / threads;
  std::vector<std::vector<double>> samples(threads);
  std::vector<std::uint64_t> bad(threads, 0);
  std::atomic<std::uint64_t> sink{0};

  const auto start = clock_type::now();
  std::vector<std::thread> readers;
  readers.reserve(threads);
  for (std::uint32_t index = 0; index < threads; ++index) {
    readers.emplace_back([&, index] {
      std::uint64_t rng = seed + index * 0x9e3779b9ULL;
      std::uint64_t local = 0;
      if (sample_latency) samples[index].reserve(per_thread);
      for (std::uint64_t op = 0; op < per_thread; ++op) {
        if (sample_latency) {
          const auto op_start = clock_type::now();
          local += graph.chase(rng, bad[index]);
          samples[index].push_back(
              std::chrono::duration<double, std::nano>(clock_type::now() -
                                                       op_start)
                  .count());
        } else {
          local += graph.chase(rng, bad[index]);
        }
      }
      sink.fetch_add(local, std::memory_order_relaxed);
    });
  }
  for (auto &reader : readers) reader.join();

  read_result result;
  result.seconds = seconds_since(start);
  result.ops = per_thread * threads;
  latency_series latencies(result.ops);
  for (std::uint32_t index = 0; index < threads; ++index) {
    for (const double sample : samples[index]) latencies.add(sample);
    result.bad_nodes += bad[index];
  }
  result.p50_ns = latencies.quantile(0.50);
  result.p99_ns = latencies.quantile(0.99);
  result.max_ns = latencies.quantile(1.0);
  return result;
}

void write_read_result(json_writer &json, const char *const key,
                       const read_result &result) {
  json.begin_object(key);
  json.key_value("ops", result.ops);
  json.key_value("seconds", result.seconds);
  json.key_value("ops_per_second",
                 result.seconds > 0.0
                     ? static_cast<double>(result.ops) / result.seconds
                     : 0.0);
  json.key_value("p50_ns", result.p50_ns);
  json.key_value("p99_ns", result.p99_ns);
  json.key_value("max_ns", result.max_ns);
  json.key_value("bad_nodes", result.bad_nodes);
  json.end_object();
}

/// \brief One checkpoint of the mixed phase.
struct checkpoint_sample {
  double seconds = 0.0;
  std::uint64_t pages_touched = 0;
  std::uint64_t blocks_touched = 0;
  std::uint64_t slots_written = 0;
  std::uint64_t store_allocated_bytes = 0;
};

template <typename Manager>
void run_arm(const options &opt, json_writer &json) {
  using traits = backend_traits<Manager>;
  const std::filesystem::path datastore = opt.root / traits::name;
  std::filesystem::remove_all(datastore);
  std::filesystem::create_directories(datastore);

  traits::configure(opt);
  traits::arm_thread();
  const std::uint64_t block_size = traits::block_size(opt);
  const std::uint64_t page = static_cast<std::uint64_t>(::getpagesize());

  json.begin_object();
  json.key_value("backend", traits::name);
  json.key_value("block_size", block_size);
  json.key_value("filesystem", filesystem_name(opt.root));
  // On a file system that clones extents, metall's snapshot shares blocks
  // instead of copying them, so the retention phase compares sharing against
  // sharing. On one that does not, it compares sharing against a copy.
  json.key_value("file_clone", supports_file_clone(opt.root));

  // ---- load ----
  std::optional<Manager> manager;
  manager.emplace(metall::create_only, datastore.c_str(), opt.capacity);
  if (!manager->check_sanity()) {
    json.key_value("error", "create failed");
    json.end_object();
    return;
  }
  page_map load_pages(manager->get_address(), opt.capacity, page);

  // Each phase measures its own resident peak. Without the reset the peak of
  // the largest phase, which is the load, would be reported for every phase
  // after it, and a resident ceiling that holds during the mixed phase would
  // be invisible.
  const bool phase_local_peak = rss_peak_reset();

  auto start = clock_type::now();
  const build_result built =
      node_graph<Manager>::build(*manager, opt.graph, load_pages);
  const double load_seconds = seconds_since(start);

  start = clock_type::now();
  manager->flush(true);
  const double load_checkpoint_seconds = seconds_since(start);
  const bool load_sane = manager->check_sanity();

  json.begin_object("load");
  json.key_value("nodes", built.nodes);
  json.key_value("roots", built.roots);
  json.key_value("bytes_allocated", built.bytes_allocated);
  json.key_value("seconds", load_seconds);
  json.key_value("bytes_per_second",
                 load_seconds > 0.0
                     ? static_cast<double>(built.bytes_allocated) / load_seconds
                     : 0.0);
  json.key_value("checkpoint_seconds", load_checkpoint_seconds);
  json.key_value("pages_touched", load_pages.pages());
  json.key_value("pages_out_of_range", load_pages.out_of_range());
  json.key_value("blocks_touched", load_pages.blocks(block_size));
  json.key_value("rss_peak_bytes", rss_peak_bytes());
  json.key_value("rss_bytes", rss_bytes());
  json.key_value("rss_peak_is_phase_local", phase_local_peak);
  json.key_value("sane", load_sane);
  write_store_size(json, "store", measure_store(datastore));
  write_statistics(json, read_statistics(*manager));
  json.end_object();

  // ---- read, datastore still open ----
  node_graph<Manager> graph(*manager);
  if (!graph.valid()) {
    json.key_value("error", "the graph is missing its named objects");
    json.end_object();
    return;
  }
  write_read_result(json, "read",
                    run_readers(graph, opt.chase_ops, opt.readers,
                                opt.graph.seed + 1, true));

  // ---- reopen ----
  manager.reset();
  start = clock_type::now();
  manager.emplace(metall::open_only, datastore.c_str());
  const double reopen_seconds = seconds_since(start);
  if (!manager->check_sanity()) {
    json.key_value("error", "reopen failed");
    json.end_object();
    return;
  }
  traits::arm_thread();
  node_graph<Manager> reopened(*manager);
  json.begin_object("reopen");
  json.key_value("seconds", reopen_seconds);
  json.end_object();
  write_read_result(json, "read_after_reopen",
                   run_readers(reopened, opt.chase_ops, opt.readers,
                               opt.graph.seed + 2, true));

  // ---- mixed: readers, a path-copying writer, a collector, checkpoints ----
  if (opt.updates > 0) {
    // A phase of its own for the resident peak, and a sampler for the resident
    // size over it, because a governor that trims holds a ceiling rather than
    // lowering a peak that was already reached.
    rss_peak_reset();
    rss_sampler mixed_rss(std::chrono::milliseconds{
        static_cast<long>(opt.rss_sample_ms)});

    std::atomic<bool> readers_run{true};
    std::atomic<std::uint64_t> reader_ops{0};
    std::atomic<std::uint64_t> reader_bad{0};
    // One chase in reader_latency_stride is timed. Timing every chase would
    // cost two clock reads on an operation of a few hundred nanoseconds and
    // would move the throughput this phase reports; a sample this dense still
    // leaves tens of thousands of points for the percentiles.
    constexpr std::uint64_t reader_latency_stride = 64;
    std::vector<std::vector<double>> reader_samples(opt.readers);
    std::vector<std::thread> readers;
    readers.reserve(opt.readers);
    for (std::uint32_t index = 0; index < opt.readers; ++index) {
      readers.emplace_back([&, index] {
        std::uint64_t rng = opt.graph.seed + 100 + index;
        std::uint64_t bad = 0;
        std::uint64_t ops = 0;
        std::uint64_t sink = 0;
        std::vector<double> &samples = reader_samples[index];
        while (readers_run.load(std::memory_order_acquire)) {
          if (ops % reader_latency_stride == 0) {
            const auto op_start = clock_type::now();
            sink += reopened.chase(rng, bad);
            samples.push_back(std::chrono::duration<double, std::nano>(
                                  clock_type::now() - op_start)
                                  .count());
          } else {
            sink += reopened.chase(rng, bad);
          }
          ++ops;
        }
        reader_ops.fetch_add(ops, std::memory_order_relaxed);
        reader_bad.fetch_add(bad, std::memory_order_relaxed);
        if (sink == 0) reader_bad.fetch_add(1, std::memory_order_relaxed);
      });
    }

    // The collector frees the versions the writer hands over. It runs while
    // the writer allocates and while checkpoints run, which is the pattern
    // the engine is built for: one writer, one collector, many readers.
    std::mutex retired_mutex;
    std::vector<std::vector<node *>> retired;
    std::atomic<bool> collector_run{true};
    std::atomic<std::uint64_t> freed_versions{0};
    std::thread collector{[&] {
      traits::arm_thread();
      for (;;) {
        std::vector<node *> batch;
        {
          std::lock_guard<std::mutex> lock(retired_mutex);
          if (!retired.empty()) {
            batch = std::move(retired.back());
            retired.pop_back();
          }
        }
        if (batch.empty()) {
          if (!collector_run.load(std::memory_order_acquire)) return;
          std::this_thread::yield();
          continue;
        }
        reopened.free_nodes(batch);
        freed_versions.fetch_add(1, std::memory_order_relaxed);
      }
    }};

    // The writer's own stores. The allocator's metadata writes and the
    // collector's frees land in the segment too and are not counted here, so
    // the dirty set is a lower bound and the amplification an upper bound.
    page_map writer_pages(manager->get_address(), opt.capacity, page);
    std::vector<checkpoint_sample> checkpoints;
    std::vector<std::vector<node *>> live(reopened.version_count());
    std::uint64_t bytes_allocated = 0;
    backend_statistics before_mixed = read_statistics(*manager);
    std::uint64_t written_at_last_checkpoint = before_mixed.slots_written;

    const auto mixed_start = clock_type::now();
    std::uint64_t rng = opt.graph.seed + 7;
    for (std::uint64_t update = 0; update < opt.updates; ++update) {
      const std::uint64_t slot =
          reopened.published() % reopened.version_count();
      std::vector<node *> created;
      bytes_allocated += reopened.update(rng, writer_pages, created);
      if (!live[slot].empty()) {
        std::lock_guard<std::mutex> lock(retired_mutex);
        retired.push_back(std::move(live[slot]));
      }
      live[slot] = std::move(created);

      if (opt.checkpoint_every > 0 && (update + 1) % opt.checkpoint_every == 0) {
        const std::uint64_t pages_touched = writer_pages.pages();
        const std::uint64_t blocks_touched = writer_pages.blocks(block_size);
        const auto checkpoint_start = clock_type::now();
        manager->flush(true);
        const double seconds = seconds_since(checkpoint_start);
        const backend_statistics now = read_statistics(*manager);
        checkpoints.push_back(
            {seconds, pages_touched, blocks_touched,
             now.slots_written - written_at_last_checkpoint,
             measure_store(datastore).datastore_allocated});
        written_at_last_checkpoint = now.slots_written;
        writer_pages.clear();
      }
    }
    const double mixed_seconds = seconds_since(mixed_start);

    collector_run.store(false, std::memory_order_release);
    collector.join();
    readers_run.store(false, std::memory_order_release);
    for (auto &reader : readers) reader.join();
    mixed_rss.stop();

    latency_series reader_latencies;
    for (const std::vector<double> &samples : reader_samples) {
      for (const double sample : samples) reader_latencies.add(sample);
    }

    latency_series checkpoint_latencies(checkpoints.size());
    std::uint64_t written_total = 0;
    std::uint64_t pages_total = 0;
    std::uint64_t blocks_total = 0;
    for (const checkpoint_sample &sample : checkpoints) {
      checkpoint_latencies.add(sample.seconds);
      written_total += sample.slots_written;
      pages_total += sample.pages_touched;
      blocks_total += sample.blocks_touched;
    }

    json.begin_object("mixed");
    json.key_value("updates", opt.updates);
    json.key_value("seconds", mixed_seconds);
    json.key_value("updates_per_second",
                   mixed_seconds > 0.0
                       ? static_cast<double>(opt.updates) / mixed_seconds
                       : 0.0);
    json.key_value("bytes_allocated", bytes_allocated);
    json.key_value("freed_versions",
                   freed_versions.load(std::memory_order_relaxed));
    json.key_value("reader_ops", reader_ops.load(std::memory_order_relaxed));
    json.key_value("reader_ops_per_second",
                   mixed_seconds > 0.0
                       ? static_cast<double>(
                             reader_ops.load(std::memory_order_relaxed)) /
                             mixed_seconds
                       : 0.0);
    json.key_value("reader_bad_nodes",
                   reader_bad.load(std::memory_order_relaxed));
    // Reader latency while the writer commits and the governor trims. This is
    // what a resident budget costs a reader: a swept page faults back in.
    json.key_value("reader_latency_samples",
                   static_cast<std::uint64_t>(reader_latencies.count()));
    json.key_value("reader_p50_ns", reader_latencies.quantile(0.50));
    json.key_value("reader_p99_ns", reader_latencies.quantile(0.99));
    json.key_value("reader_max_ns", reader_latencies.quantile(1.0));
    json.key_value("checkpoints", static_cast<std::uint64_t>(checkpoints.size()));
    json.key_value("checkpoint_p50_seconds", checkpoint_latencies.quantile(0.5));
    json.key_value("checkpoint_p99_seconds",
                   checkpoint_latencies.quantile(0.99));
    json.key_value("checkpoint_max_seconds", checkpoint_latencies.quantile(1.0));
    json.key_value("checkpoint_mean_seconds", checkpoint_latencies.mean());
    // The dirty set the workload produced, against what the backend wrote for
    // it. pages_touched counts only the stores this benchmark makes, so the
    // dirty set is a lower bound and the amplification an upper bound. Only a
    // block-granular backend reports the written side, so the default arm
    // carries no amplification number.
    json.key_value("pages_touched_total", pages_total);
    json.key_value("dirty_bytes_total", pages_total * page);
    if (block_size > 0) {
      json.key_value("blocks_touched_total", blocks_total);
      json.key_value("blocks_written_total", written_total);
      json.key_value("written_bytes_total", written_total * block_size);
      if (pages_total > 0) {
        json.key_value("write_amplification",
                       static_cast<double>(written_total * block_size) /
                           static_cast<double>(pages_total * page));
      }
    }
    json.key_value("rss_peak_bytes", rss_peak_bytes());
    json.key_value("rss_bytes", rss_bytes());
    json.key_value("rss_samples", mixed_rss.samples());
    json.key_value("rss_sampled_max_bytes", mixed_rss.max_bytes());
    json.key_value("rss_sampled_mean_bytes", mixed_rss.mean_bytes());
    write_store_size(json, "store", measure_store(datastore));
    write_statistics(json, read_statistics(*manager));
    json.begin_array("checkpoint_samples");
    for (const checkpoint_sample &sample : checkpoints) {
      json.begin_object();
      json.key_value("seconds", sample.seconds);
      json.key_value("pages_touched", sample.pages_touched);
      json.key_value("blocks_touched", sample.blocks_touched);
      json.key_value("blocks_written", sample.slots_written);
      json.key_value("store_allocated_bytes", sample.store_allocated_bytes);
      json.end_object();
    }
    json.end_array();
    json.end_object();
  }

  // ---- retention: a series of snapshots, then deleting the oldest ----
  // The writer and the collector are done, so the datastore holds one version
  // and a snapshot is a consistent one. metall documents snapshot() as single
  // threaded, and the default backend copies files, so a concurrent writer
  // would make the two arms measure different things.
  if (opt.snapshots > 0) {
    json.begin_array("retention");
    std::vector<std::filesystem::path> series;
    for (std::uint32_t index = 0; index < opt.snapshots; ++index) {
      const std::filesystem::path snapshot =
          opt.root / (std::string(traits::name) + "-snap-" +
                      std::to_string(index));
      std::filesystem::remove_all(snapshot);

      flush_filesystem(opt.root);
      const std::uint64_t free_before = filesystem_free_bytes(opt.root);
      auto snapshot_start = clock_type::now();
      const bool taken = manager->snapshot(snapshot.c_str());
      const double snapshot_seconds = seconds_since(snapshot_start);
      flush_filesystem(opt.root);
      const std::uint64_t free_after = filesystem_free_bytes(opt.root);
      series.push_back(snapshot);

      disk_usage total;
      total.add_tree(datastore);
      for (const std::filesystem::path &path : series) total.add_tree(path);

      double delete_seconds = 0.0;
      bool deleted = false;
      std::uint64_t freed_by_delete = 0;
      if (series.size() > opt.retain) {
        const std::filesystem::path oldest = series.front();
        const auto delete_start = clock_type::now();
        deleted = Manager::remove(oldest.c_str());
        delete_seconds = seconds_since(delete_start);
        flush_filesystem(opt.root);
        const std::uint64_t free_now = filesystem_free_bytes(opt.root);
        freed_by_delete = free_now > free_after ? free_now - free_after : 0;
        series.erase(series.begin());
      }

      json.begin_object();
      json.key_value("index", static_cast<std::uint64_t>(index));
      json.key_value("taken", taken);
      json.key_value("snapshot_seconds", snapshot_seconds);
      // What the snapshot really cost the file system. Per-file accounting
      // cannot see extent sharing, so on a cloning file system it overstates
      // the cost; this does not.
      json.key_value("filesystem_cost_bytes",
                     free_before > free_after ? free_before - free_after : 0);
      json.key_value("unique_allocated_bytes", total.allocated_bytes());
      json.key_value("unique_apparent_bytes", total.apparent_bytes());
      json.key_value("unique_files", total.files());
      json.key_value("deleted_oldest", deleted);
      json.key_value("delete_seconds", delete_seconds);
      json.key_value("delete_freed_bytes", freed_by_delete);
      json.end_object();
    }
    json.end_array();
    for (const std::filesystem::path &path : series) {
      std::filesystem::remove_all(path);
    }
  }

  json.key_value("sane_at_end", manager->check_sanity());
  manager.reset();
  json.end_object();
}

std::uint64_t parse_size(const std::string &text) {
  char *end = nullptr;
  std::uint64_t value = std::strtoull(text.c_str(), &end, 10);
  if (end != nullptr && *end != '\0') {
    switch (*end) {
      case 'k':
      case 'K':
        value <<= 10;
        break;
      case 'm':
      case 'M':
        value <<= 20;
        break;
      case 'g':
      case 'G':
        value <<= 30;
        break;
      default:
        break;
    }
  }
  return value;
}

void print_usage() {
  std::cout
      << "run_segment_backend_bench [options]\n"
         "  --root <dir>              where the datastores go\n"
         "  --backend default|privateer|both\n"
         "  --nodes <n>               nodes in the base graph\n"
         "  --levels <n>              levels of the base graph\n"
         "  --versions <n>            versions the writer keeps live\n"
         "  --capacity <bytes>        segment capacity, k/m/g suffixes\n"
         "  --readers <n>             reader threads\n"
         "  --chase-ops <n>           chases per read phase\n"
         "  --updates <n>             path-copy updates, 0 skips the phase\n"
         "  --checkpoint-every <n>    updates per checkpoint\n"
         "  --snapshots <n>           snapshots, 0 skips the phase\n"
         "  --retain <n>              snapshots kept before the oldest goes\n"
         "  --block-size <bytes>      privateer arm, 0 keeps the default\n"
         "  --cleaner off|non_durable|eager_durable\n"
         "  --cleaner-interval-ms <n>  sweep cadence, 0 the default\n"
         "  --cleaner-batch-slots <n>  slots per batch, 0 the default\n"
         "  --dirty-soft/--dirty-low/--dirty-hard <bytes>\n"
         "  --resident-soft/--resident-low <bytes>  resident budget, Linux\n"
         "  --sweep-interval-ms <n>   resident sweep cadence, 0 the default\n"
         "  --rss-sample-ms <n>       resident sampling in the mixed phase\n"
         "  --seed <n>                workload seed\n"
         "  --json <file>             write the results there, - for stdout\n"
         "  --quick                   small sizes, for a smoke run\n";
}

}  // namespace

int main(int argc, char **argv) {
  options opt;
  for (int index = 1; index < argc; ++index) {
    const std::string flag = argv[index];
    const auto value = [&]() -> std::string {
      if (index + 1 >= argc) {
        std::cerr << flag << " needs a value\n";
        std::exit(1);
      }
      return argv[++index];
    };
    if (flag == "--root") {
      opt.root = value();
    } else if (flag == "--backend") {
      const std::string backend = value();
      opt.run_default = backend == "default" || backend == "both";
      opt.run_privateer = backend == "privateer" || backend == "both";
    } else if (flag == "--nodes") {
      opt.graph.nodes = parse_size(value());
    } else if (flag == "--levels") {
      opt.graph.levels = static_cast<std::uint32_t>(parse_size(value()));
    } else if (flag == "--versions") {
      opt.graph.versions = static_cast<std::uint32_t>(parse_size(value()));
    } else if (flag == "--seed") {
      opt.graph.seed = parse_size(value());
    } else if (flag == "--capacity") {
      opt.capacity = parse_size(value());
    } else if (flag == "--readers") {
      opt.readers = static_cast<std::uint32_t>(parse_size(value()));
    } else if (flag == "--chase-ops") {
      opt.chase_ops = parse_size(value());
    } else if (flag == "--updates") {
      opt.updates = parse_size(value());
    } else if (flag == "--checkpoint-every") {
      opt.checkpoint_every = parse_size(value());
    } else if (flag == "--snapshots") {
      opt.snapshots = static_cast<std::uint32_t>(parse_size(value()));
    } else if (flag == "--retain") {
      opt.retain = static_cast<std::uint32_t>(parse_size(value()));
    } else if (flag == "--block-size") {
      opt.block_size = parse_size(value());
    } else if (flag == "--cleaner") {
      opt.cleaner = value();
    } else if (flag == "--cleaner-interval-ms") {
      opt.cleaner_interval_ms = parse_size(value());
    } else if (flag == "--cleaner-batch-slots") {
      opt.cleaner_batch_slots = parse_size(value());
    } else if (flag == "--dirty-soft") {
      opt.dirty_soft = parse_size(value());
    } else if (flag == "--dirty-low") {
      opt.dirty_low = parse_size(value());
    } else if (flag == "--dirty-hard") {
      opt.dirty_hard = parse_size(value());
    } else if (flag == "--resident-soft") {
      opt.resident_soft = parse_size(value());
    } else if (flag == "--resident-low") {
      opt.resident_low = parse_size(value());
    } else if (flag == "--sweep-interval-ms") {
      opt.sweep_interval_ms = parse_size(value());
    } else if (flag == "--rss-sample-ms") {
      opt.rss_sample_ms = parse_size(value());
    } else if (flag == "--json") {
      opt.json_path = value();
    } else if (flag == "--quick") {
      opt.graph.nodes = 200000;
      opt.capacity = 1ULL << 30;
      opt.chase_ops = 20000;
      opt.updates = 2000;
      opt.checkpoint_every = 500;
      opt.snapshots = 3;
      opt.retain = 2;
    } else if (flag == "--help" || flag == "-h") {
      print_usage();
      return 0;
    } else {
      std::cerr << "unknown flag " << flag << "\n";
      print_usage();
      return 1;
    }
  }

#ifndef METALL_BENCH_WITH_PRIVATEER
  if (opt.run_privateer) {
    std::cerr << "built without the privateer backend, running the default arm"
                 " only\n";
    opt.run_privateer = false;
  }
#endif

  std::filesystem::create_directories(opt.root);

  std::ofstream file;
  std::ostream *out = &std::cout;
  if (!opt.json_path.empty() && opt.json_path != "-") {
    file.open(opt.json_path);
    if (!file) {
      std::cerr << "cannot write " << opt.json_path << "\n";
      return 1;
    }
    out = &file;
  }

  json_writer json(*out);
  json.begin_object();
  json.key_value("nodes", opt.graph.nodes);
  json.key_value("levels", static_cast<std::uint64_t>(opt.graph.levels));
  json.key_value("versions", static_cast<std::uint64_t>(opt.graph.versions));
  json.key_value("capacity", opt.capacity);
  json.key_value("readers", static_cast<std::uint64_t>(opt.readers));
  json.key_value("chase_ops", opt.chase_ops);
  json.key_value("updates", opt.updates);
  json.key_value("checkpoint_every", opt.checkpoint_every);
  json.key_value("snapshots", static_cast<std::uint64_t>(opt.snapshots));
  json.key_value("retain", static_cast<std::uint64_t>(opt.retain));
  json.key_value("cleaner", opt.cleaner);
  json.key_value("cleaner_interval_ms", opt.cleaner_interval_ms);
  json.key_value("cleaner_batch_slots", opt.cleaner_batch_slots);
  json.key_value("dirty_soft", opt.dirty_soft);
  json.key_value("dirty_low", opt.dirty_low);
  json.key_value("dirty_hard", opt.dirty_hard);
  json.key_value("resident_soft", opt.resident_soft);
  json.key_value("resident_low", opt.resident_low);
  json.key_value("sweep_interval_ms", opt.sweep_interval_ms);
  json.key_value("hardware_threads",
                 static_cast<std::uint64_t>(std::thread::hardware_concurrency()));
  // Two arms in one process are a smoke run, not a comparison: the first arm
  // leaves its datastore and its snapshots in the page cache and its resident
  // high water mark in the process, and the second arm pays for both.
  json.key_value("arms_in_one_process", opt.run_default && opt.run_privateer);
  json.begin_array("arms");
  if (opt.run_default) run_arm<metall::manager>(opt, json);
#ifdef METALL_BENCH_WITH_PRIVATEER
  if (opt.run_privateer) run_arm<metall::manager_privateer>(opt, json);
#endif
  json.end_array();
  json.end_object();
  *out << "\n";
  return 0;
}
