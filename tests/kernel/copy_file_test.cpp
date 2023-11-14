// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <dice/metall/metall.hpp>

#include <dice/metall/detail/file.hpp>
#include "../test_utility.hpp"

namespace {

void create(const std::string &dir_path) {
  dice::metall::manager manager(dice::metall::create_only, dir_path);

  manager.construct<uint32_t>("a")(1);
  manager.construct<uint64_t>("b")(2);
}

void open(const std::string &dir_path) {
  dice::metall::manager manager(dice::metall::open_read_only, dir_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

const std::filesystem::path &original_dir_path() {
  const static std::filesystem::path path(test_utility::make_test_path("/original"));
  return path;
}

const std::filesystem::path &copy_dir_path() {
  const static std::filesystem::path path(test_utility::make_test_path("/copy"));
  return path;
}

TEST(CopyFileTest, SyncCopy) {
  dice::metall::manager::remove(original_dir_path());
  dice::metall::manager::remove(copy_dir_path());

  create(original_dir_path());

  ASSERT_TRUE(dice::metall::manager::copy(original_dir_path(),
                                    copy_dir_path()));

  open(copy_dir_path());
}

TEST(CopyFileTest, AsyncCopy) {
  dice::metall::manager::remove(original_dir_path());
  dice::metall::manager::remove(copy_dir_path());

  create(original_dir_path());

  auto handler = dice::metall::manager::copy_async(original_dir_path(),
                                             copy_dir_path());
  ASSERT_TRUE(handler.get());

  open(copy_dir_path());
}
}  // namespace