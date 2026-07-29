// Copyright 2025 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_SEGMENT_BACKEND_NODE_GRAPH_HPP
#define METALL_BENCH_SEGMENT_BACKEND_NODE_GRAPH_HPP

// The workload: a directed acyclic graph of small nodes, in the shape a
// hypertrie has. Nodes are allocated one by one, they point at each other
// with offset pointers, a read is a root to leaf pointer chase, and a write
// is a path copy that leaves the nodes it replaces untouched.
//
// The graph has two parts. The base graph is built once and never modified,
// which is what readers walk: MVCC readers hold a version, so the nodes of a
// version stay valid while they read. The version ring holds the roots the
// writer produces; a version that leaves the ring has its own nodes freed,
// and those nodes are only ever the ones the writer allocated. Readers
// therefore never see a freed node, and the concurrency of the phase is
// still the real one: a writer dirtying pages, a garbage collector freeing,
// and readers faulting in pages, all at the same time.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <metall/offset_ptr.hpp>

#include "measure.hpp"

namespace metall::bench::segment_backend {

/// \brief Children per node. Nodes use between one and this many.
inline constexpr std::uint32_t k_fanout = 8;

/// \brief One graph node. The payload follows the fixed head; payload_words
/// says how many words of it the allocation really holds.
struct node {
  std::uint64_t key;
  std::uint32_t children;
  std::uint32_t payload_words;
  metall::offset_ptr<node> child[k_fanout];
  std::uint64_t payload[1];
};

static_assert(sizeof(metall::offset_ptr<node>) == sizeof(std::uint64_t),
              "an offset pointer is expected to be one word");
static_assert(sizeof(node) == 16 + k_fanout * sizeof(std::uint64_t) +
                                  sizeof(std::uint64_t),
              "the node head is expected to be free of padding");

inline constexpr std::size_t k_node_head_bytes =
    sizeof(node) - sizeof(std::uint64_t);

inline std::size_t node_bytes(const std::uint32_t payload_words) {
  return k_node_head_bytes +
         static_cast<std::size_t>(payload_words) * sizeof(std::uint64_t);
}

/// \brief splitmix64. Deterministic, so a seed reproduces a run.
inline std::uint64_t next_random(std::uint64_t &state) {
  state += 0x9e3779b97f4a7c15ULL;
  std::uint64_t value = state;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

/// \brief Payload size of the next node. Most nodes are small, a few are
/// large, which is what a hypertrie node graph looks like.
inline std::uint32_t draw_payload_words(std::uint64_t &rng) {
  const std::uint64_t roll = next_random(rng) % 100;
  if (roll < 70) return 2;
  if (roll < 95) return 16;
  return 256;
}

struct graph_config {
  std::uint64_t nodes = 4000000;
  std::uint32_t levels = 4;
  std::uint64_t seed = 0x5eed;
  /// \brief Roots the writer keeps live. A version that leaves the ring is
  /// freed, so this is how long a version stays readable.
  std::uint32_t versions = 64;
};

struct build_result {
  std::uint64_t nodes = 0;
  std::uint64_t bytes_allocated = 0;
  std::uint64_t roots = 0;
};

/// \brief The graph in one open datastore. Construct it on a datastore the
/// build phase filled; it looks up the named objects and nothing else.
template <typename Manager>
class node_graph {
 public:
  using node_pointer = metall::offset_ptr<node>;

  static constexpr const char *k_roots_name = "roots";
  static constexpr const char *k_versions_name = "versions";

  /// \brief Fills a freshly created datastore. Level 0 holds leaves; every
  /// later level points at fanout nodes of the level below, chosen at random,
  /// so nodes are shared the way a hypertrie shares them. The top level is
  /// the root table.
  static build_result build(Manager &manager, const graph_config &config,
                            page_map &pages) {
    build_result result;
    std::uint64_t rng = config.seed;
    const std::uint64_t per_level =
        config.nodes / (config.levels == 0 ? 1 : config.levels);

    std::vector<node *> below;
    std::vector<node *> current;
    for (std::uint32_t level = 0; level < config.levels; ++level) {
      current.clear();
      current.reserve(per_level);
      for (std::uint64_t index = 0; index < per_level; ++index) {
        const std::uint32_t payload_words = draw_payload_words(rng);
        const std::size_t bytes = node_bytes(payload_words);
        auto *const created = static_cast<node *>(manager.allocate(bytes));
        if (created == nullptr) return result;

        created->key = next_random(rng) | 1;  // never zero, so a read can tell
        created->payload_words = payload_words;
        created->children = below.empty()
                                ? 0
                                : 1 + static_cast<std::uint32_t>(
                                          next_random(rng) % k_fanout);
        for (std::uint32_t slot = 0; slot < k_fanout; ++slot) {
          created->child[slot] =
              slot < created->children
                  ? below[next_random(rng) % below.size()]
                  : nullptr;
        }
        for (std::uint32_t word = 0; word < payload_words; ++word) {
          created->payload[word] = created->key + word;
        }
        pages.touch(created, bytes);

        current.push_back(created);
        ++result.nodes;
        result.bytes_allocated += bytes;
      }
      below.swap(current);
    }

    auto *const roots = manager.template construct<node_pointer>(
        k_roots_name)[below.size()]();
    if (roots == nullptr) return result;
    for (std::size_t index = 0; index < below.size(); ++index) {
      roots[index] = below[index];
    }
    pages.touch(roots, below.size() * sizeof(node_pointer));
    result.roots = below.size();

    auto *const versions = manager.template construct<node_pointer>(
        k_versions_name)[config.versions]();
    if (versions == nullptr) return result;
    pages.touch(versions, config.versions * sizeof(node_pointer));
    return result;
  }

  explicit node_graph(Manager &manager) : m_manager(&manager) {
    const auto roots = manager.template find<node_pointer>(k_roots_name);
    m_roots = roots.first;
    m_root_count = roots.second;
    const auto versions = manager.template find<node_pointer>(k_versions_name);
    m_versions = versions.first;
    m_version_count = versions.second;
  }

  [[nodiscard]] bool valid() const {
    return m_roots != nullptr && m_root_count > 0 && m_versions != nullptr &&
           m_version_count > 0;
  }

  [[nodiscard]] std::uint64_t root_count() const { return m_root_count; }

  /// \brief One root to leaf chase. Reads every payload word on the way and
  /// returns their sum, so nothing can be optimized away. A node with a zero
  /// key means the read found something the write path never wrote.
  [[nodiscard]] std::uint64_t chase(std::uint64_t &rng,
                                    std::uint64_t &bad_nodes) const {
    const node *current = metall::to_raw_pointer(
        m_roots[next_random(rng) % m_root_count]);
    std::uint64_t sum = 0;
    while (current != nullptr) {
      if (current->key == 0) ++bad_nodes;
      for (std::uint32_t word = 0; word < current->payload_words; ++word) {
        sum += current->payload[word];
      }
      if (current->children == 0) break;
      current = metall::to_raw_pointer(
          current->child[next_random(rng) % current->children]);
    }
    return sum;
  }

  /// \brief One path copy. Walks a random path of the base graph, allocates a
  /// new node per level bottom up, and publishes the new top node in the
  /// version ring. Returns the bytes it allocated; the nodes it created are
  /// appended to created, which is what frees the version later.
  std::uint64_t update(std::uint64_t &rng, page_map &pages,
                       std::vector<node *> &created) {
    // walk down, recording which child of each node the path took
    std::vector<std::pair<node *, std::uint32_t>> path;
    path.reserve(8);
    node *current =
        metall::to_raw_pointer(m_roots[next_random(rng) % m_root_count]);
    while (current != nullptr) {
      if (current->children == 0) {
        path.emplace_back(current, k_fanout);  // leaf, no child taken
        break;
      }
      const auto taken =
          static_cast<std::uint32_t>(next_random(rng) % current->children);
      path.emplace_back(current, taken);
      current = metall::to_raw_pointer(current->child[taken]);
    }

    // copy bottom up, so each copy can point at the copy below it
    std::uint64_t allocated = 0;
    node *below = nullptr;
    for (std::size_t step = path.size(); step > 0; --step) {
      node *const source = path[step - 1].first;
      const std::uint32_t taken = path[step - 1].second;
      const std::size_t bytes = node_bytes(source->payload_words);
      auto *const copy = static_cast<node *>(m_manager->allocate(bytes));
      if (copy == nullptr) break;

      copy->key = next_random(rng) | 1;
      copy->children = source->children;
      copy->payload_words = source->payload_words;
      for (std::uint32_t slot = 0; slot < k_fanout; ++slot) {
        copy->child[slot] = source->child[slot];
      }
      if (below != nullptr && taken < k_fanout) copy->child[taken] = below;
      for (std::uint32_t word = 0; word < source->payload_words; ++word) {
        copy->payload[word] = copy->key + word;
      }
      pages.touch(copy, bytes);

      created.push_back(copy);
      allocated += bytes;
      below = copy;
    }

    if (below != nullptr) {
      const std::uint64_t slot = m_published % m_version_count;
      m_versions[slot] = below;
      pages.touch(&m_versions[slot], sizeof(node_pointer));
      ++m_published;
    }
    return allocated;
  }

  /// \brief The ring slot the next update overwrites, so its version has to be
  /// freed. Empty until the ring is full once.
  [[nodiscard]] std::uint64_t published() const { return m_published; }
  [[nodiscard]] std::uint64_t version_count() const { return m_version_count; }

  void free_nodes(const std::vector<node *> &nodes) {
    for (node *const created : nodes) {
      m_manager->deallocate(created);
    }
  }

 private:
  Manager *m_manager{nullptr};
  node_pointer *m_roots{nullptr};
  std::size_t m_root_count{0};
  node_pointer *m_versions{nullptr};
  std::size_t m_version_count{0};
  std::uint64_t m_published{0};
};

}  // namespace metall::bench::segment_backend

#endif  // METALL_BENCH_SEGMENT_BACKEND_NODE_GRAPH_HPP
