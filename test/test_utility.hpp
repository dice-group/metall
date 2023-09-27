// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TEST_UTILITY_HPP
#define METALL_TEST_UTILITY_HPP

#include "gtest/gtest.h"

#include <string>
#include <cstdlib>

#include <metall/detail/file.hpp>

namespace test_utility {

const char *k_test_dir_env_name = "METALL_TEST_DIR";
const char *k_default_test_dir = "/tmp/metall_test_dir";

namespace detail {
inline std::filesystem::path get_test_dir() {
  if (const char *env_p = std::getenv(k_test_dir_env_name)) {
    return env_p;
  }
  return k_default_test_dir;
}
}  // namespace detail

inline void create_test_dir() {
  if (!std::filesystem::exists(detail::get_test_dir())) {
    std::filesystem::create_directory(detail::get_test_dir());
  }
}

inline std::filesystem::path make_test_path(const std::string &name = std::string()) {
  return detail::get_test_dir() /
         (std::string{"metalltest-"} + ::testing::UnitTest::GetInstance()->current_test_case()->name() + "-" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name() + "-" +
         name);
}

}  // namespace test_utility
#endif  // METALL_TEST_UTILITY_HPP
