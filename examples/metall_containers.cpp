// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/metall/metall.hpp>
#include <dice/metall/container/deque.hpp>
#include <dice/metall/container/list.hpp>
#include <dice/metall/container/map.hpp>
#include <dice/metall/container/set.hpp>
#include <dice/metall/container/unordered_map.hpp>
#include <dice/metall/container/unordered_set.hpp>
#include <dice/metall/container/vector.hpp>
#include <dice/metall/container/stack.hpp>
#include <dice/metall/container/queue.hpp>
#include <dice/metall/container/priority_queue.hpp>
#include <dice/metall/container/string.hpp>

// Boost 1.81 or later is required
#if BOOST_VERSION >= 108100
#include <dice/metall/container/unordered_flat_map.hpp>
#include <dice/metall/container/unordered_flat_set.hpp>
#endif

// Boost 1.82 or later is required
#if BOOST_VERSION >= 108200
#include <dice/metall/container/unordered_node_map.hpp>
#include <dice/metall/container/unordered_node_set.hpp>
#endif

using namespace dice::copperr;
namespace mc = dice::copperr::container;

int main() {
  manager mg(create_only, "/tmp/dir");

  auto *deque = mg.construct<mc::deque<int>>("deque")(mg.get_allocator());
  mg.destroy_ptr(deque);

  mg.construct<mc::list<int>>("list")(mg.get_allocator());

  mg.construct<mc::map<int, int>>("map")(mg.get_allocator());
  mg.construct<mc::multimap<int, int>>("mmap")(mg.get_allocator());

  mg.construct<mc::set<int>>("set")(mg.get_allocator());
  mg.construct<mc::multiset<int>>("multiset")(mg.get_allocator());

  mg.construct<mc::unordered_map<int, int>>("unordered_map")(
      mg.get_allocator());
  mg.construct<mc::unordered_multimap<int, int>>("unordered_multimap")(
      mg.get_allocator());

  mg.construct<mc::unordered_set<int>>("unordered_set")(mg.get_allocator());
  mg.construct<mc::unordered_multiset<int>>("unordered_multiset")(
      mg.get_allocator());

#if BOOST_VERSION >= 108100
  mg.construct<mc::unordered_flat_map<int, int>>("unordered_flat_map")(
      mg.get_allocator());
  mg.construct<mc::unordered_flat_set<int>>("unordered_flat_set")(
      mg.get_allocator());
#endif

#if BOOST_VERSION >= 108200
  mg.construct<mc::unordered_node_map<int, int>>("unordered_node_map")(
      mg.get_allocator());
  mg.construct<mc::unordered_node_set<int>>("unordered_node_set")(
      mg.get_allocator());
#endif

  mg.construct<mc::vector<int>>("vector")(mg.get_allocator());

  mg.construct<mc::stack<int>>("stack")(mg.get_allocator());

  mg.construct<mc::queue<int>>("queue")(mg.get_allocator());

  mg.construct<mc::priority_queue<int>>("priority_queue")(mg.get_allocator());

  mg.construct<mc::string>("string")(mg.get_allocator());
  mg.construct<mc::wstring>("wstring")(mg.get_allocator());

  using innger_set = mc::set<int>;
  using vec_of_sets =
      mc::vector<innger_set, manager::scoped_allocator_type<innger_set>>;
  mg.construct<vec_of_sets>("vec-sets")(mg.get_allocator());

  return 0;
}