// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_HASH_HPP
#define METALL_UTILITY_HASH_HPP

#include <dice/metall/detail/hash.hpp>

namespace dice::copperr::utility {

/// \brief Hash a value of type T.
/// \tparam T Data type to hash.
/// If void is specified, the hash data type is determined by () operator.
/// \tparam seed A seed value used for hashing.
template <typename T = void, unsigned int seed = 123>
using hash = dice::copperr::mtlldetail::hash<seed>;

/// \brief Hash function for std::string-compatible string container.
/// \tparam seed A seed value used for hashing.
template <unsigned int seed = 123>
using str_hash = dice::copperr::mtlldetail::str_hash<seed>;

}  // namespace dice::copperr::utility

#endif  // METALL_UTILITY_HASH_HPP
