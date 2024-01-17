// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
#define METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP

#include <dice/metall/kernel/manager_kernel_fwd.hpp>

namespace dice::copperr {
namespace kernel {

template <typename chunk_no_type, std::size_t k_chunk_size>
template <typename out_stream_type>
void manager_kernel<chunk_no_type, k_chunk_size>::profile(
    out_stream_type *log_out) {
  m_segment_memory_allocator.profile(log_out);
}

}  // namespace kernel
}  // namespace dice::copperr

#endif  // METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
