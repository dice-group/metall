// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_SEGMENT_HEADER_HPP
#define METALL_KERNEL_SEGMENT_HEADER_HPP

namespace dice::copperr::kernel {

struct segment_header {
  void *manager_kernel_address;

  ~segment_header() noexcept { manager_kernel_address = nullptr; }
};

}  // namespace dice::copperr::kernel

#endif  // METALL_KERNEL_SEGMENT_HEADER_HPP
