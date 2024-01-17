// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_MANAGER_KERNEL_FWD_HPP
#define METALL_KERNEL_MANAGER_KERNEL_FWD_HPP

namespace dice::copperr {
namespace kernel {

/// \brief Manager kernel class version 0
/// \tparam chunk_no_type Type of chunk number
/// \tparam chunk_size Size of single chunk in byte
template <typename _chunk_no_type, std::size_t _chunk_size>
class manager_kernel;

}  // namespace kernel
}  // namespace dice::copperr

#endif  // METALL_KERNEL_MANAGER_KERNEL_FWD_HPP
