// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <cstring>
#include <string>

#include <metall/ext/privateer.hpp>
#include <metall/detail/file.hpp>
#include "../test_utility.hpp"

namespace {
using segment_storage_type = metall::privateer_segment_storage;

const std::string &test_dir() {
  const static std::string path(test_utility::make_test_path());
  return path;
}

std::string base_path(const std::string &name) {
  return test_dir() + "/" + name;
}

void prepare_test_dir() {
  ASSERT_TRUE(metall::mtlldetail::remove_file(test_dir()));
  ASSERT_TRUE(metall::mtlldetail::create_directory(test_dir()));
}

// One block of the engine's default configuration. Test capacities are
// multiples of it, so extend() reaches them exactly.
std::size_t block_size() {
  return metall::privateer_segment_storage::options().block_size.value_or(
      privateer::default_block_size);
}

void fill(void *const addr, const std::size_t nbytes, const char value) {
  std::memset(addr, value, nbytes);
}

void expect_filled(const void *const addr, const std::size_t nbytes,
                   const char value) {
  const auto *buf = static_cast<const char *>(addr);
  for (std::size_t i = 0; i < nbytes; ++i) {
    ASSERT_EQ(buf[i], value) << "at byte " << i;
  }
}

TEST(PrivateerSegmentStorageTest, PageSize) {
  segment_storage_type storage;
  ASSERT_GT(storage.page_size(), 0);
}

TEST(PrivateerSegmentStorageTest, ArmCallingThread) {
  // Idempotent per thread, and callable before any segment exists.
  ASSERT_TRUE(segment_storage_type::arm_calling_thread());
  ASSERT_TRUE(segment_storage_type::arm_calling_thread());
}

TEST(PrivateerSegmentStorageTest, Create) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 4;

  segment_storage_type storage;
  ASSERT_TRUE(storage.create(base_path("store"), capacity));
  ASSERT_TRUE(storage.is_open());
  ASSERT_TRUE(storage.check_sanity());
  ASSERT_FALSE(storage.read_only());
  ASSERT_NE(storage.get_segment(), nullptr);

  // The segment header sits in its own memory ahead of the segment.
  ASSERT_NE(static_cast<void *>(&storage.get_segment_header()),
            storage.get_segment());
  ASSERT_LT(static_cast<void *>(&storage.get_segment_header()),
            storage.get_segment());

  // A segment always holds at least one block.
  ASSERT_GE(storage.size(), block_size());

  ASSERT_TRUE(storage.extend(capacity));
  fill(storage.get_segment(), capacity, '1');
  expect_filled(storage.get_segment(), capacity, '1');
}

TEST(PrivateerSegmentStorageTest, Extend) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;

  segment_storage_type storage;
  ASSERT_TRUE(storage.create(base_path("store"), capacity));

  // Has enough space already.
  ASSERT_TRUE(storage.extend(1));
  ASSERT_GE(storage.size(), block_size());

  // A partial block is rounded up to a whole one.
  ASSERT_TRUE(storage.extend(block_size() + 1));
  ASSERT_EQ(storage.size(), block_size() * 2);

  // Beyond the capacity the request fails and the segment is unchanged.
  ASSERT_FALSE(storage.extend(capacity + block_size()));
  ASSERT_EQ(storage.size(), capacity);
  ASSERT_TRUE(storage.check_sanity());
  ASSERT_TRUE(storage.is_open());

  // The storage still works after the failed request.
  fill(storage.get_segment(), capacity, '1');
  ASSERT_TRUE(storage.sync(true));
}

TEST(PrivateerSegmentStorageTest, SyncMakesDataVisibleToTheNextOpen) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;
  const std::string path = base_path("store");

  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.create(path, capacity));
    ASSERT_TRUE(storage.extend(capacity));
    fill(storage.get_segment(), capacity, '1');
    ASSERT_TRUE(storage.sync(true));
    ASSERT_TRUE(storage.release());
  }

  // Open and update.
  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.open(path, capacity, false));
    ASSERT_TRUE(storage.is_open());
    ASSERT_TRUE(storage.check_sanity());
    ASSERT_FALSE(storage.read_only());
    ASSERT_EQ(storage.size(), capacity);
    expect_filled(storage.get_segment(), capacity, '1');

    fill(storage.get_segment(), capacity, '2');
    ASSERT_TRUE(storage.sync(true));
    ASSERT_TRUE(storage.release());
  }

  // Read only.
  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.open(path, capacity, true));
    ASSERT_TRUE(storage.is_open());
    ASSERT_TRUE(storage.check_sanity());
    ASSERT_TRUE(storage.read_only());
    ASSERT_EQ(storage.size(), capacity);
    expect_filled(storage.get_segment(), capacity, '2');

    // A read-only segment grows for nobody.
    ASSERT_FALSE(storage.extend(capacity + block_size()));
    ASSERT_TRUE(storage.release());
  }
}

TEST(PrivateerSegmentStorageTest, WritesAfterTheLastSyncAreNotPersisted) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;
  const std::string path = base_path("store");

  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.create(path, capacity));
    ASSERT_TRUE(storage.extend(capacity));
    fill(storage.get_segment(), capacity, '1');
    ASSERT_TRUE(storage.sync(true));

    // Not synchronized, so the datastore keeps the state above.
    fill(storage.get_segment(), block_size(), '2');
    ASSERT_TRUE(storage.release());
  }

  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.open(path, capacity, true));
    expect_filled(storage.get_segment(), capacity, '1');
  }
}

TEST(PrivateerSegmentStorageTest, Snapshot) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;
  const std::string source = base_path("store");
  const std::string destination = base_path("snapshot");

  segment_storage_type storage;
  ASSERT_TRUE(storage.create(source, capacity));
  ASSERT_TRUE(storage.extend(capacity));
  fill(storage.get_segment(), capacity, '1');
  ASSERT_TRUE(storage.snapshot(destination, false, 1));

  // The snapshot holds the state at the time it was taken, and the source
  // stays writable afterwards.
  fill(storage.get_segment(), capacity, '2');
  ASSERT_TRUE(storage.sync(true));
  ASSERT_TRUE(storage.release());

  {
    segment_storage_type snapshot;
    ASSERT_TRUE(snapshot.open(destination, capacity, true));
    ASSERT_EQ(snapshot.size(), capacity);
    expect_filled(snapshot.get_segment(), capacity, '1');
  }

  {
    segment_storage_type reopened;
    ASSERT_TRUE(reopened.open(source, capacity, true));
    expect_filled(reopened.get_segment(), capacity, '2');
  }
}

TEST(PrivateerSegmentStorageTest, Copy) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;
  const std::string source = base_path("store");
  const std::string destination = base_path("copy");

  {
    segment_storage_type storage;
    ASSERT_TRUE(storage.create(source, capacity));
    ASSERT_TRUE(storage.extend(capacity));
    fill(storage.get_segment(), capacity, '1');
    ASSERT_TRUE(storage.sync(true));
    ASSERT_TRUE(storage.release());
  }

  ASSERT_TRUE(segment_storage_type::copy(source, destination, false, 1));

  {
    segment_storage_type copy;
    ASSERT_TRUE(copy.open(destination, capacity, false));
    ASSERT_EQ(copy.size(), capacity);
    expect_filled(copy.get_segment(), capacity, '1');

    // The copy is independent: writing it does not touch the source.
    fill(copy.get_segment(), capacity, '2');
    ASSERT_TRUE(copy.sync(true));
    ASSERT_TRUE(copy.release());
  }

  {
    segment_storage_type original;
    ASSERT_TRUE(original.open(source, capacity, true));
    expect_filled(original.get_segment(), capacity, '1');
  }
}

TEST(PrivateerSegmentStorageTest, FreeRegionZeroesWholeBlocks) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;

  segment_storage_type storage;
  ASSERT_TRUE(storage.create(base_path("store"), capacity));
  ASSERT_TRUE(storage.extend(capacity));
  fill(storage.get_segment(), capacity, '1');

  ASSERT_TRUE(storage.free_region(0, block_size()));
  expect_filled(storage.get_segment(), block_size(), '\0');
  expect_filled(static_cast<char *>(storage.get_segment()) + block_size(),
                block_size(), '1');

  // A range that covers no whole block keeps its content.
  ASSERT_TRUE(storage.free_region(block_size(), block_size() / 2));
  expect_filled(static_cast<char *>(storage.get_segment()) + block_size(),
                block_size(), '1');

  ASSERT_TRUE(storage.sync(true));
  ASSERT_TRUE(storage.check_sanity());
}

TEST(PrivateerSegmentStorageTest, OpenRefusesAMissingDatastore) {
  prepare_test_dir();

  segment_storage_type storage;
  ASSERT_FALSE(storage.open(base_path("absent"), block_size(), false));
  ASSERT_FALSE(storage.is_open());
  // A failed open leaves the instance unusable, as metall expects.
  ASSERT_FALSE(storage.check_sanity());
}

TEST(PrivateerSegmentStorageTest, MoveKeepsTheSegment) {
  prepare_test_dir();
  const std::size_t capacity = block_size() * 2;

  segment_storage_type source;
  ASSERT_TRUE(source.create(base_path("store"), capacity));
  ASSERT_TRUE(source.extend(capacity));
  fill(source.get_segment(), capacity, '1');
  void *const segment = source.get_segment();

  segment_storage_type moved(std::move(source));
  ASSERT_TRUE(moved.is_open());
  ASSERT_EQ(moved.get_segment(), segment);
  ASSERT_EQ(moved.size(), capacity);
  expect_filled(moved.get_segment(), capacity, '1');

  ASSERT_FALSE(source.is_open());
  ASSERT_FALSE(source.check_sanity());

  ASSERT_TRUE(moved.sync(true));
  ASSERT_TRUE(moved.release());
}
}  // namespace
