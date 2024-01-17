// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <dice/copperr/copperr.hpp>
#include <dice/copperr/kernel/object_size_manager.hpp>

static constexpr std::size_t k_max_segment_size = 1ULL << 48ULL;
using object_size_manager =
    dice::copperr::kernel::object_size_manager<dice::copperr::manager::chunk_size(),
                                        k_max_segment_size>;

int main() {
  std::cout << "Bin number,\tSize,\tMax Internal Fragmentation Size"
            << std::endl;
  std::size_t pre_size = 8;
  for (std::size_t i = 0; i < object_size_manager::num_sizes(); ++i) {
    const auto size = object_size_manager::at(i);
    if (i == 0 || i >= object_size_manager::num_small_sizes())
      std::cout << i << "\t" << size << "\tN/A" << std::endl;
    else
      std::cout << i << "\t" << size << "\t"
                << static_cast<double>(size - pre_size - 1) / static_cast<double>(pre_size + 1) << std::endl;
    pre_size = size;
  }

  return 0;
}