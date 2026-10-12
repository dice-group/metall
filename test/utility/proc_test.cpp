// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <thread>

#include <metall/detail/proc.hpp>

namespace {

TEST(ProcTest, NumAvailableCpus) {
  ASSERT_GE(metall::mtlldetail::get_num_available_cpus(), 1U);
}

#if defined(__linux__) && defined(_GNU_SOURCE)
// The one-CPU and the two-CPU case run in their own threads, so that the
// affinity mask of the main thread stays as it is.
TEST(ProcTest, NumAvailableCpusFollowsAffinity) {
  cpu_set_t original_set;
  CPU_ZERO(&original_set);
  ASSERT_EQ(::sched_getaffinity(0, sizeof(original_set), &original_set), 0);
  const int num_original_cpus = CPU_COUNT(&original_set);
  ASSERT_EQ(metall::mtlldetail::get_num_available_cpus(),
            static_cast<unsigned int>(num_original_cpus));

  std::thread one_cpu([&original_set]() {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for (int cpu_no = 0; cpu_no < CPU_SETSIZE; ++cpu_no) {
      if (CPU_ISSET(cpu_no, &original_set)) {
        CPU_SET(cpu_no, &cpu_set);
        break;
      }
    }
    ASSERT_EQ(::sched_setaffinity(0, sizeof(cpu_set), &cpu_set), 0);
    ASSERT_EQ(metall::mtlldetail::get_num_available_cpus(), 1U);
  });
  one_cpu.join();

  if (num_original_cpus < 2) {
    return;
  }
  std::thread two_cpus([&original_set]() {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    int num_set = 0;
    for (int cpu_no = 0; cpu_no < CPU_SETSIZE && num_set < 2; ++cpu_no) {
      if (CPU_ISSET(cpu_no, &original_set)) {
        CPU_SET(cpu_no, &cpu_set);
        ++num_set;
      }
    }
    ASSERT_EQ(::sched_setaffinity(0, sizeof(cpu_set), &cpu_set), 0);
    ASSERT_EQ(metall::mtlldetail::get_num_available_cpus(), 2U);
  });
  two_cpus.join();
}
#endif

}  // namespace
