// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PARAMETER_PACK_HPP
#define METALL_DETAIL_UTILITY_PARAMETER_PACK_HPP

namespace dice::metall::mtlldetail {

// ---------- get index ---------- //
template <typename...>
struct get_index;

// Found
template <typename T, typename... List>
struct get_index<T, T, List...> {
  static constexpr std::size_t value = 0;
};

// Searching
template <typename T, typename F, typename... List>
struct get_index<T, F, List...> {
  static constexpr std::size_t value = get_index<T, List...>::value + 1;
};

}  // namespace dice::metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_PARAMETER_PACK_HPP
