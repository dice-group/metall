// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PROC_HPP
#define METALL_DETAIL_UTILITY_PROC_HPP

#include <sched.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

#ifdef _GNU_SOURCE
#include <sys/sysinfo.h>
#define SUPPORT_GET_CPU_NO true
#else
#define SUPPORT_GET_CPU_NO false
#endif

#include <bitset>
#include <cstddef>
#include <thread>

namespace metall::mtlldetail {

/// \brief Returns the number of the logical CPU core on which the calling
/// thread is currently executing.
inline unsigned int get_cpu_no() {
#if SUPPORT_GET_CPU_NO

  const int cpu = ::sched_getcpu();
  if (cpu == -1) {
    return 0;
  }
  return cpu;

#else

#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "CPU number is always 0"
#endif
  return 0;

#endif
}

/// \brief Returns the number of the logical CPU cores on the system.
inline unsigned int get_num_cpus() {
#if SUPPORT_GET_CPU_NO
  return ::get_nprocs_conf();
#else
  return std::thread::hardware_concurrency();
#endif
}

/// \brief Returns the number of the logical CPU cores the calling thread may
/// run on. On Linux, this is the CPU count of the affinity mask of the calling
/// thread, read with the `sched_getaffinity` system call. If that call fails,
/// or on other POSIX systems (macOS included), it is the number of online CPUs
/// from `sysconf(_SC_NPROCESSORS_ONLN)`. macOS has no affinity mask for a
/// thread. If that fails too, it is `std::thread::hardware_concurrency()`.
/// \return The number of CPUs. At least 1.
inline unsigned int get_num_available_cpus() {
#if defined(__linux__)
  // The raw system call works with every libc and does not need `cpu_set_t`,
  // which holds only 1024 CPUs. The mask holds 4096 CPUs. On success, the call
  // returns the number of bytes of the mask the kernel wrote.
  constexpr std::size_t k_num_bits_per_word = 8 * sizeof(unsigned long);
  unsigned long mask[4096 / k_num_bits_per_word] = {};
  const long num_bytes =
      ::syscall(SYS_sched_getaffinity, 0, sizeof(mask), mask);
  if (num_bytes > 0) {
    const std::size_t num_words =
        static_cast<std::size_t>(num_bytes) / sizeof(unsigned long);
    std::size_t count = 0;
    for (std::size_t i = 0; i < num_words; ++i) {
      count += std::bitset<k_num_bits_per_word>(mask[i]).count();
    }
    if (count > 0) {
      return static_cast<unsigned int>(count);
    }
  }
#endif

#if defined(_SC_NPROCESSORS_ONLN)
  const long num_online = ::sysconf(_SC_NPROCESSORS_ONLN);
  if (num_online > 0) {
    return static_cast<unsigned int>(num_online);
  }
#endif

  const unsigned int count = std::thread::hardware_concurrency();
  return count > 0 ? count : 1;
}

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_PROC_HPP
