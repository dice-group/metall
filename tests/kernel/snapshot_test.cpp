// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <string>

#include <dice/copperr/copperr.hpp>

#include "../test_utility.hpp"

namespace {

std::filesystem::path original_dir_path() {
  const std::filesystem::path path(test_utility::make_test_path("original"));
  return path;
}

std::filesystem::path snapshot_dir_path() {
  const std::filesystem::path path(test_utility::make_test_path("snapshot"));
  return path;
}

TEST(SnapshotTest, Snapshot) {
  dice::copperr::manager::remove(original_dir_path());
  dice::copperr::manager::remove(snapshot_dir_path());

  {
    dice::copperr::manager manager(dice::copperr::create_only, original_dir_path());

    [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
    [[maybe_unused]] auto b =
        manager.construct<uint64_t>(dice::copperr::unique_instance)(2);

    ASSERT_TRUE(manager.snapshot(snapshot_dir_path()));
    ASSERT_TRUE(dice::copperr::manager::consistent(snapshot_dir_path()));

    // UUID
    const auto original_uuid =
        dice::copperr::manager::get_uuid(original_dir_path());
    ASSERT_FALSE(original_uuid.empty());
    const auto snapshot_uuid =
        dice::copperr::manager::get_uuid(snapshot_dir_path());
    ASSERT_FALSE(snapshot_uuid.empty());
    ASSERT_NE(original_uuid, snapshot_uuid);

    // Version
    ASSERT_EQ(dice::copperr::manager::get_version(original_dir_path()),
              dice::copperr::manager::get_version(snapshot_dir_path()));
  }

  {
    dice::copperr::manager manager(dice::copperr::open_read_only,
                            snapshot_dir_path());

    auto a = manager.find<uint32_t>("a").first;
    ASSERT_EQ(*a, 1);

    auto b = manager.find<uint64_t>(dice::copperr::unique_instance).first;
    ASSERT_EQ(*b, 2);
  }
}
}  // namespace