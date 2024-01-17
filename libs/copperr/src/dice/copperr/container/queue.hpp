// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_QUEUE_HPP
#define METALL_CONTAINER_QUEUE_HPP

#include <queue>

#include <dice/copperr/container/deque.hpp>

namespace dice::copperr::container {

/// \brief A queue container that uses Metall as its default allocator.
template <typename T, typename Container = deque<T>>
using queue = std::queue<T, Container>;

}  // namespace dice::copperr::container

#endif  // METALL_CONTAINER_QUEUE_HPP
