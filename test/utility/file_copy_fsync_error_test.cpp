// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This executable defines fsync(2). The calls to fsync in the Metall headers
// go to this definition, which fails on request. Thus, this test is a separate
// executable.

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // For RTLD_NEXT
#endif

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <metall/detail/file.hpp>

#include "../test_utility.hpp"

namespace {
std::atomic<bool> fail_fsync{false};
std::atomic<int> num_failed_fsync_calls{0};
}  // namespace

// If fail_fsync is true, this function fails with EIO.
// If fail_fsync is false, this function calls the fsync of the C library.
extern "C" int fsync(int fd) {
  if (fail_fsync.load()) {
    ++num_failed_fsync_calls;
    errno = EIO;
    return -1;
  }

  using fsync_function = int (*)(int);
  static const auto real_fsync =
      reinterpret_cast<fsync_function>(::dlsym(RTLD_NEXT, "fsync"));
  if (!real_fsync) {
    errno = ENOSYS;
    return -1;
  }
  return real_fsync(fd);
}

namespace {

namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;

std::string read_file(const fs::path &path) {
  std::ifstream ifs(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

bool write_file(const fs::path &path, const std::string &content) {
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
  ofs << content;
  ofs.close();
  return !!ofs;
}

fs::path prepare_test_dir(const std::string &name) {
  test_utility::create_test_dir();
  const auto dir = test_utility::make_test_path(name);
  mdtl::remove_file(dir);
  mdtl::create_directory(dir);
  return dir;
}

TEST(FileCopyFsyncErrorTest, CopyFileReportsFsyncError) {
  const auto dir = prepare_test_dir("copy");
  const auto source = dir / "source";
  const auto destination = dir / "destination";
  const std::string content(1 << 20, 'x');
  ASSERT_TRUE(write_file(source, content));

  for (const bool sparse_copy : {true, false}) {
    SCOPED_TRACE(sparse_copy ? "copy_file(sparse_copy=true)"
                             : "copy_file(sparse_copy=false)");

    // The copy succeeds if fsync works.
    ASSERT_TRUE(mdtl::remove_file(destination));
    ASSERT_TRUE(mdtl::copy_file(source, destination, sparse_copy));
    ASSERT_TRUE(read_file(destination) == content);

    // The copy fails if fsync fails.
    ASSERT_TRUE(mdtl::remove_file(destination));
    num_failed_fsync_calls = 0;
    fail_fsync = true;
    const bool copied = mdtl::copy_file(source, destination, sparse_copy);
    fail_fsync = false;
    EXPECT_FALSE(copied) << "copy_file returned true, but fsync failed "
                         << num_failed_fsync_calls << " time(s)";
  }
}

}  // namespace
