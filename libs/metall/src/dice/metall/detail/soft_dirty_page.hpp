// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
#define METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP

#include <cstdint>
#include <iostream>
#include <fstream>
#include <dice/metall/detail/memory.hpp>
#include <dice/metall/logger.hpp>

namespace dice::copperr::mtlldetail {

inline bool reset_soft_dirty_bit() {
  std::ofstream ofs("/proc/self/clear_refs");
  if (!ofs.is_open()) {
    METALL_ERROR("Cannot open file clear_refs");
    return false;
  }

  ofs << "4";
  ofs.close();

  if (!ofs) {
    METALL_ERROR("Cannot write to /proc/self/clear_refs");
    return false;
  }

  return true;
}

inline constexpr bool check_soft_dirty_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 55ULL) & 1ULL;
}

inline constexpr bool check_swapped_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 62ULL) & 1ULL;
}

inline constexpr bool check_present_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 63ULL) & 1ULL;
}

}  // namespace dice::copperr::mtlldetail

#endif  // METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
