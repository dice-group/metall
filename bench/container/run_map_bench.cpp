// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks the STL map container using different allocators.
/// Usage:
/// ./run_map_bench
/// # modify the values in the main(), if needed.

#include <iostream>
#include <map>
#include <boost/container/map.hpp>
#include <dice/copperr/container/map.hpp>
#include <dice/copperr/copperr.hpp>
#include <dice/copperr/detail/time.hpp>

#include "bench_common.hpp"

int main() {
  std::size_t scale = 17;
  std::size_t num_inputs = (1ULL << scale) * 16;
  std::vector<std::pair<uint64_t, uint64_t>> inputs;

  // gen_edges(scale, num_inputs, inputs);
  gen_random_values(num_inputs, inputs);
  std::cout << "Generated inputs\t" << inputs.size() << std::endl;

  {
    std::map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "map took (s)\t" << elapsed_time << std::endl;
  }

  {
    boost::container::map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost map took (s)\t" << elapsed_time << std::endl;
  }

  {
    dice::copperr::manager mngr(dice::copperr::create_only, "/tmp/copperr");
    dice::copperr::container::map<uint64_t, uint64_t> map(mngr.get_allocator());

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost map with Metall took (s)\t" << elapsed_time
              << std::endl;
  }

  return 0;
}