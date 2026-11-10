// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This executable defines `copy_file_range(2)`. The calls to `copy_file_range`
// in the Metall headers go to this definition, which can fail or copy no data
// on request. Thus, this test is a separate executable. It is for Linux only.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // For `copy_file_range` and `RTLD_NEXT`
#endif

#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <metall/detail/file.hpp>

#include "../test_utility.hpp"

namespace {

constexpr std::size_t k_mib = std::size_t(1) << 20;

/// \brief Selects the result of the `copy_file_range` function of this
/// executable.
enum class copy_file_range_mode {
  /// Calls `copy_file_range` of the C library.
  real,
  /// Fails with `EXDEV`. `copy_file_range` does this across file systems.
  exdev,
  /// Copies only the bytes before 1 MiB, and returns 0 at or after 1 MiB.
  /// The kernel does this when its cache has a source size of 1 MiB, but
  /// `lseek(SEEK_DATA)` finds data after 1 MiB (for example, on FUSE).
  zero_after_1mib,
  /// Fails with `EXDEV` before 1 MiB, and returns 0 at or after 1 MiB.
  exdev_and_zero_after_1mib,
};

std::atomic<copy_file_range_mode> current_mode{copy_file_range_mode::real};

/// \brief Counts the calls of `copy_file_range`. A child process shares the
/// memory of the counts with its parent. Thus, the parent can read the counts
/// after a signal stops the child.
struct call_counts {
  std::atomic<unsigned long> calls{0};
  std::atomic<unsigned long> zero_results{0};
};

call_counts *make_shared_call_counts() {
  void *const memory =
      ::mmap(nullptr, sizeof(call_counts), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  return (memory == MAP_FAILED) ? nullptr : new (memory) call_counts();
}

call_counts *const shared_counts = make_shared_call_counts();

}  // namespace

// Gives the result that `current_mode` selects.
extern "C" ssize_t copy_file_range(int fd_in, off_t *off_in, int fd_out,
                                   off_t *off_out, size_t len,
                                   unsigned int flags) {
  if (shared_counts) {
    ++shared_counts->calls;
  }
  const copy_file_range_mode mode = current_mode.load();
  if (mode == copy_file_range_mode::exdev) {
    errno = EXDEV;
    return -1;
  }
  if (mode == copy_file_range_mode::zero_after_1mib ||
      mode == copy_file_range_mode::exdev_and_zero_after_1mib) {
    const off_t offset = off_in ? *off_in : ::lseek(fd_in, 0, SEEK_CUR);
    if (offset >= static_cast<off_t>(k_mib)) {
      if (shared_counts) {
        ++shared_counts->zero_results;
      }
      return 0;
    }
    if (mode == copy_file_range_mode::exdev_and_zero_after_1mib) {
      errno = EXDEV;
      return -1;
    }
    len = std::min(len, k_mib - static_cast<std::size_t>(offset));
  }

  using function_type =
      ssize_t (*)(int, off_t *, int, off_t *, size_t, unsigned int);
  static const auto real_copy_file_range =
      reinterpret_cast<function_type>(::dlsym(RTLD_NEXT, "copy_file_range"));
  if (!real_copy_file_range) {
    errno = ENOSYS;
    return -1;
  }
  return real_copy_file_range(fd_in, off_in, fd_out, off_out, len, flags);
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

/// \brief Makes a content that has no zero bytes.
std::string make_content(const std::size_t size) {
  std::string content(size, '\0');
  for (std::size_t i = 0; i < size; ++i) {
    content[i] = static_cast<char>('a' + i % 26);
  }
  return content;
}

/// \brief Gives the size of the file at `path` for a failure message.
std::string describe_file(const fs::path &path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return "not a regular file";
  }
  const std::string data = read_file(path);
  std::stringstream ss;
  ss << "a regular file of " << data.size() << " bytes, "
     << std::count(data.begin(), data.end(), '\0') << " of them zero";
  return ss.str();
}

/// \brief Makes a file of `file_size` bytes that has data only at the offsets
/// in `data_offsets`. Each data extent is a copy of `extent`. The other bytes
/// of the file are holes.
bool write_sparse_file(const fs::path &path, const off_t file_size,
                       const std::vector<off_t> &data_offsets,
                       const std::string &extent) {
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    return false;
  }
  bool written = true;
  for (const off_t offset : data_offsets) {
    written = written && ::pwrite(fd, extent.data(), extent.size(), offset) ==
                             ssize_t(extent.size());
  }
  written = written && ::ftruncate(fd, file_size) == 0;
  return (::close(fd) == 0) && written;
}

bool same_content(const fs::path &path1, const fs::path &path2) {
  std::ifstream ifs1(path1, std::ios::binary);
  std::ifstream ifs2(path2, std::ios::binary);
  if (!ifs1 || !ifs2) {
    return false;
  }
  return std::equal(
      std::istreambuf_iterator<char>(ifs1), std::istreambuf_iterator<char>(),
      std::istreambuf_iterator<char>(ifs2), std::istreambuf_iterator<char>());
}

/// \brief Gives the offset of the first data after the first hole of the file
/// at `path`. On error, or if there is no such data, gives -1.
off_t data_after_first_hole(const fs::path &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return -1;
  }
  const off_t hole = ::lseek(fd, 0, SEEK_HOLE);
  const off_t data = (hole == -1) ? -1 : ::lseek(fd, hole, SEEK_DATA);
  ::close(fd);
  return data;
}

/// \brief Sets the mode of `copy_file_range` in this process until the end of
/// the scope.
class scoped_copy_file_range_mode {
 public:
  explicit scoped_copy_file_range_mode(const copy_file_range_mode mode) {
    current_mode = mode;
  }

  ~scoped_copy_file_range_mode() { current_mode = copy_file_range_mode::real; }

  scoped_copy_file_range_mode(const scoped_copy_file_range_mode &) = delete;
  scoped_copy_file_range_mode &operator=(const scoped_copy_file_range_mode &) =
      delete;
};

/// \brief Closes a file descriptor at the end of the scope.
struct fd_closer {
  int fd;
  ~fd_closer() {
    if (fd != -1) {
      ::close(fd);
    }
  }
};

/// \brief The time limit in seconds for a call in a child process.
constexpr unsigned int k_time_limit = 5;

/// \brief The end of a call of `copy_file_sparse_linux` in a child process.
struct child_result {
  std::string error;   ///< Not empty if the test could not do the call.
  int signal{0};       ///< The signal that stopped the child, or 0.
  bool copied{false};  ///< The value that `copy_file_sparse_linux` returned.
};

/// \brief Calls `copy_file_sparse_linux` from `source` to `destination` in a
/// child process, with `mode` for `copy_file_range`. If the call does not
/// return in `k_time_limit` seconds, the SIGALRM signal stops the child.
child_result sparse_copy_in_child(const fs::path &source,
                                  const fs::path &destination,
                                  const copy_file_range_mode mode) {
  constexpr int returned_false = 10;
  constexpr int returned_true = 11;

  const fd_closer src{::open(source.c_str(), O_RDONLY)};
  const fd_closer dst{::open(destination.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                             S_IRUSR | S_IWUSR)};
  struct stat src_stat{};
  if (src.fd == -1 || dst.fd == -1 || ::fstat(src.fd, &src_stat) != 0) {
    return {std::string("open or fstat: ") + std::strerror(errno)};
  }
  if (!shared_counts) {
    return {"mmap for the counts of copy_file_range failed"};
  }
  shared_counts->calls = 0;
  shared_counts->zero_results = 0;

  const pid_t pid = ::fork();
  if (pid == -1) {
    return {std::string("fork: ") + std::strerror(errno)};
  }
  if (pid == 0) {
    ::signal(SIGALRM, SIG_DFL);
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    ::sigprocmask(SIG_UNBLOCK, &alarm_set, nullptr);
    current_mode = mode;
    ::alarm(k_time_limit);
    const bool copied = mdtl::fcpdtl::copy_file_sparse_linux(
        src.fd, dst.fd, static_cast<std::size_t>(src_stat.st_size));
    ::_exit(copied ? returned_true : returned_false);
  }

  int status = 0;
  pid_t waited = -1;
  do {
    waited = ::waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);
  if (waited != pid) {
    return {std::string("waitpid: ") + std::strerror(errno)};
  }
  child_result result;
  if (WIFSIGNALED(status)) {
    result.signal = WTERMSIG(status);
  } else if (WIFEXITED(status) && (WEXITSTATUS(status) == returned_false ||
                                   WEXITSTATUS(status) == returned_true)) {
    result.copied = (WEXITSTATUS(status) == returned_true);
  } else {
    result.error = "The child process ended with the wait status " +
                   std::to_string(status);
  }
  return result;
}

/// \brief Gives the counts of the last call in a child process for a failure
/// message.
std::string describe_calls() {
  std::stringstream ss;
  ss << "copy_file_range returned 0 in " << shared_counts->zero_results.load()
     << " of " << shared_counts->calls.load() << " calls";
  return ss.str();
}

TEST(FileCopyRangeTest, SparseCopyStopsWhenCopyFileRangeReturnsZero) {
  const auto dir = prepare_test_dir("zero_result");
  const auto source = dir / "source";
  const auto destination = dir / "destination";

  // A file of 2 MiB without holes. `copy_file_range` copies the first 1 MiB
  // and then returns 0, but `lseek(SEEK_DATA)` finds data at 1 MiB.
  const std::string content = make_content(2 * k_mib);
  ASSERT_TRUE(write_file(source, content));

  const child_result result = sparse_copy_in_child(
      source, destination, copy_file_range_mode::zero_after_1mib);
  ASSERT_TRUE(result.error.empty()) << result.error;
  ASSERT_NE(result.signal, SIGALRM)
      << "copy_file_sparse_linux did not return in " << k_time_limit
      << " seconds, and the SIGALRM signal stopped it. " << describe_calls()
      << ". After a result of 0, the loop calls copy_file_range again at the "
         "same offset.";
  ASSERT_EQ(result.signal, 0)
      << "The child process stopped with signal " << result.signal;
  EXPECT_FALSE(result.copied) << "copy_file_sparse_linux returned true, but "
                                 "it did not copy the data after 1 MiB";
  EXPECT_EQ(read_file(destination).compare(0, k_mib, content, 0, k_mib), 0)
      << "The destination does not start with the first 1 MiB of the source. "
      << "The destination is " << describe_file(destination);
}

TEST(FileCopyRangeTest, SparseCopyWithoutCopyFileRangeKeepsHoles) {
  const auto dir = prepare_test_dir("exdev");
  const auto source = dir / "source";
  const auto destination = dir / "destination";

  // A file of 64 MiB with 3 MiB of data in three extents.
  constexpr off_t file_size = off_t(64) << 20;
  constexpr off_t extent_size = off_t(1) << 20;
  const std::vector<off_t> data_offsets = {0, 16 * extent_size,
                                           40 * extent_size};
  ASSERT_TRUE(write_sparse_file(source, file_size, data_offsets,
                                make_content(extent_size)))
      << source << ": " << std::strerror(errno);
  struct stat source_stat{};
  ASSERT_EQ(::stat(source.c_str(), &source_stat), 0) << std::strerror(errno);
  const off_t source_allocated = off_t(source_stat.st_blocks) * 512;
  if (source_allocated >= file_size / 2) {
    GTEST_SKIP() << "The file system of " << dir << " has no holes";
  }

  bool copied = false;
  {
    // `copy_file_range` fails with `EXDEV`, as it does across file systems.
    const scoped_copy_file_range_mode mode(copy_file_range_mode::exdev);
    copied = mdtl::copy_file(source, destination, mdtl::copy_flags::sparse);
  }
  EXPECT_TRUE(copied);
  EXPECT_TRUE(same_content(source, destination))
      << "The destination has other data than the source";

  struct stat destination_stat{};
  ASSERT_EQ(::stat(destination.c_str(), &destination_stat), 0)
      << std::strerror(errno);
  const off_t destination_allocated = off_t(destination_stat.st_blocks) * 512;
  EXPECT_EQ(destination_stat.st_size, file_size);
  EXPECT_LT(destination_allocated, file_size / 2)
      << "The destination allocates " << destination_allocated
      << " bytes for a file of " << file_size << " bytes. The source "
      << "allocates " << source_allocated << " bytes. Thus, the copy wrote "
      << "the holes as data.";
}

TEST(FileCopyRangeTest, SparseCopyHandlesExdevAndZeroTogether) {
  const auto dir = prepare_test_dir("exdev_and_zero");
  const auto source = dir / "source";
  const auto destination = dir / "destination";

  // A file of 4 MiB with two data extents of 64 KiB, at 0 and at 2 MiB.
  // `copy_file_range` fails with `EXDEV` for the first extent and returns 0
  // for the second extent.
  constexpr off_t file_size = off_t(4) << 20;
  constexpr off_t second_offset = off_t(2) << 20;
  const std::string extent = make_content(64 << 10);
  ASSERT_TRUE(write_sparse_file(source, file_size, {0, second_offset}, extent))
      << source << ": " << std::strerror(errno);
  if (data_after_first_hole(source) != second_offset) {
    GTEST_SKIP() << "The file system of " << dir
                 << " shows no hole between the two extents";
  }

  const child_result result = sparse_copy_in_child(
      source, destination, copy_file_range_mode::exdev_and_zero_after_1mib);
  ASSERT_TRUE(result.error.empty()) << result.error;
  ASSERT_NE(result.signal, SIGALRM)
      << "copy_file_sparse_linux did not return in " << k_time_limit
      << " seconds, and the SIGALRM signal stopped it. " << describe_calls()
      << ".";
  ASSERT_EQ(result.signal, 0)
      << "The child process stopped with signal " << result.signal;
  EXPECT_FALSE(result.copied) << "copy_file_sparse_linux returned true, but "
                                 "it did not copy the second extent";
  EXPECT_EQ(read_file(destination).compare(0, extent.size(), extent), 0)
      << "copy_file_range failed with EXDEV for the first extent, and the "
      << "destination does not have the data of this extent. The destination "
      << "is " << describe_file(destination);
}

}  // namespace
