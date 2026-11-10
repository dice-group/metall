// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <metall/detail/file.hpp>

#include "../test_utility.hpp"

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
/// A sparse copy must write all bytes of this content.
std::string make_content(const std::size_t size) {
  std::string content(size, '\0');
  for (std::size_t i = 0; i < size; ++i) {
    content[i] = static_cast<char>('a' + i % 26);
  }
  return content;
}

/// \brief Gives the type and the size of the file at `path` for a failure
/// message.
std::string describe_file(const fs::path &path) {
  std::error_code ec;
  const auto status = fs::symlink_status(path, ec);
  if (ec || !fs::exists(status)) {
    return "no file";
  }
  if (fs::is_directory(status)) {
    return "a directory";
  }
  if (!fs::is_regular_file(status)) {
    return "a file that is not a regular file";
  }
  const std::string data = read_file(path);
  std::stringstream ss;
  ss << "a regular file of " << data.size() << " bytes, "
     << std::count(data.begin(), data.end(), '\0') << " of them zero";
  return ss.str();
}

struct copy_mode {
  std::string name;
  mdtl::copy_flags flags;
};

const copy_mode k_sparse{"copy_flags::sparse", mdtl::copy_flags::sparse};
const copy_mode k_dense{"copy_flags::dense", mdtl::copy_flags::dense};
const copy_mode k_sparse_clone{
    "copy_flags::sparse | copy_flags::clone",
    mdtl::copy_flags::sparse | mdtl::copy_flags::clone};

TEST(FileCopyTest, CopyToSameFileKeepsData) {
  const auto dir = prepare_test_dir("same_file");
  const auto file = dir / "file";
  const auto hard_link = dir / "hard_link";
  const auto symbolic_link = dir / "symbolic_link";
  const std::string content = make_content(64 * 1024 + 123);

  // Each destination is a name of the source file.
  const std::vector<std::pair<std::string, fs::path>> destinations = {
      {"the source path", file},
      {"a hard link to the source", hard_link},
      {"a symbolic link to the source", symbolic_link}};

  for (const auto &mode : {k_sparse, k_dense, k_sparse_clone}) {
    for (const auto &[destination_name, destination] : destinations) {
      SCOPED_TRACE(mode.name + ", destination is " + destination_name);

      // A failed case destroys the source file. Thus, make all files again.
      ASSERT_TRUE(mdtl::remove_file(file));
      ASSERT_TRUE(mdtl::remove_file(hard_link));
      ASSERT_TRUE(mdtl::remove_file(symbolic_link));
      ASSERT_TRUE(write_file(file, content));
      std::error_code ec;
      fs::create_hard_link(file, hard_link, ec);
      ASSERT_FALSE(ec) << ec.message();
      fs::create_symlink(file.filename(), symbolic_link, ec);
      ASSERT_FALSE(ec) << ec.message();

      const bool copied = mdtl::copy_file(file, destination, mode.flags);

      // A copy of a file onto itself is not possible. Thus, copy_file
      // returns false. A clone can replace the destination with a clone of
      // the source. Then the data stays, and copy_file can return true.
      if (!(mode.flags & mdtl::copy_flags::clone)) {
        EXPECT_FALSE(copied) << "copy_file returned true";
      }

      const auto size = fs::file_size(file, ec);
      if (ec) {
        ADD_FAILURE() << "The source file is gone: " << ec.message();
        continue;
      }
      const std::string data = read_file(file);
      EXPECT_TRUE(data == content)
          << "The source file has " << size << " bytes, and "
          << std::count(data.begin(), data.end(), '\0')
          << " of them are zero. Before the copy, it had " << content.size()
          << " bytes and no zero byte.";
    }
  }
}

TEST(FileCopyTest, CopyFromMissingSourceKeepsDestination) {
  const auto dir = prepare_test_dir("missing_source");
  const auto source = dir / "missing_source";
  const auto destination = dir / "destination";
  const std::string old_content = "old data\n";

  for (const auto &mode : {k_sparse, k_dense, k_sparse_clone}) {
    SCOPED_TRACE(mode.name);
    ASSERT_TRUE(write_file(destination, old_content));
    ASSERT_FALSE(mdtl::file_exist(source));

    EXPECT_FALSE(mdtl::copy_file(source, destination, mode.flags));
    EXPECT_EQ(read_file(destination), old_content)
        << "The destination is " << describe_file(destination);
  }
}

TEST(FileCopyTest, DirectorySourceLeavesDestinationUnchanged) {
  const auto dir = prepare_test_dir("directory_source");
  const auto source = dir / "source";
  ASSERT_TRUE(mdtl::create_directory(source / "sub"));
  ASSERT_TRUE(write_file(source / "file", "file data\n"));

  const auto new_destination = dir / "new_destination";
  const auto existing_destination = dir / "existing_destination";
  const std::string old_content = "old data\n";

  for (const auto &mode : {k_sparse, k_dense}) {
    SCOPED_TRACE(mode.name);

    // copy_file fails and does not make a destination.
    ASSERT_TRUE(mdtl::remove_file(new_destination));
    EXPECT_FALSE(mdtl::copy_file(source, new_destination, mode.flags));
    EXPECT_FALSE(mdtl::file_exist(new_destination))
        << "copy_file made " << describe_file(new_destination);

    // copy_file fails and does not change an existing destination.
    ASSERT_TRUE(write_file(existing_destination, old_content));
    EXPECT_FALSE(mdtl::copy_file(source, existing_destination, mode.flags));
    EXPECT_EQ(read_file(existing_destination), old_content)
        << "The destination is " << describe_file(existing_destination);
  }
}

TEST(FileCopyTest, FifoSourceFailsWithoutBlocking) {
  const auto dir = prepare_test_dir("fifo_source");
  const auto source = dir / "fifo";
  const auto destination = dir / "destination";
  if (::mkfifo(source.c_str(), S_IRUSR | S_IWUSR) != 0) {
    GTEST_SKIP() << "mkfifo: " << std::strerror(errno);
  }

  // No process opens the FIFO to write. Thus, an open of the FIFO to read
  // waits. A child process calls copy_file. If copy_file waits for more than
  // time_limit seconds, the SIGALRM signal stops the child process.
  constexpr unsigned int time_limit = 2;
  const pid_t pid = ::fork();
  ASSERT_NE(pid, -1) << "fork: " << std::strerror(errno);
  if (pid == 0) {
    ::signal(SIGALRM, SIG_DFL);
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    ::sigprocmask(SIG_UNBLOCK, &alarm_set, nullptr);
    ::alarm(time_limit);
    const bool copied =
        mdtl::copy_file(source, destination, mdtl::copy_flags::sparse);
    ::_exit(copied ? 1 : 0);
  }

  int status = 0;
  pid_t waited = -1;
  do {
    waited = ::waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);
  ASSERT_EQ(waited, pid) << "waitpid: " << std::strerror(errno);
  const bool blocked = WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM;
  ASSERT_FALSE(blocked)
      << "copy_file did not return in " << time_limit
      << " seconds. It waits for a process that opens the FIFO to write.";
  const bool exited = WIFEXITED(status);
  ASSERT_TRUE(exited) << "The child process stopped with signal "
                      << WTERMSIG(status);
  const bool copied = WEXITSTATUS(status) != 0;
  EXPECT_FALSE(copied) << "copy_file returned true";
  EXPECT_FALSE(mdtl::file_exist(destination))
      << "copy_file made " << describe_file(destination);
}

/// \brief Limits the size of the files that this process writes.
/// A write past the limit fails with EFBIG and does not stop the process.
/// The destructor restores the previous limit and the previous SIGXFSZ action.
class scoped_file_size_limit {
 public:
  explicit scoped_file_size_limit(const rlim_t max_file_size) {
    struct sigaction ignore_action{};
    ignore_action.sa_handler = SIG_IGN;
    sigemptyset(&ignore_action.sa_mask);
    if (::sigaction(SIGXFSZ, &ignore_action, &m_old_action) != 0) {
      return;
    }
    m_action_changed = true;

    if (::getrlimit(RLIMIT_FSIZE, &m_old_limit) != 0) {
      return;
    }
    struct rlimit limit = m_old_limit;
    limit.rlim_cur = std::min(max_file_size, m_old_limit.rlim_max);
    m_limit_changed = (::setrlimit(RLIMIT_FSIZE, &limit) == 0);
  }

  ~scoped_file_size_limit() {
    if (m_limit_changed) {
      ::setrlimit(RLIMIT_FSIZE, &m_old_limit);
    }
    if (m_action_changed) {
      ::sigaction(SIGXFSZ, &m_old_action, nullptr);
    }
  }

  scoped_file_size_limit(const scoped_file_size_limit &) = delete;
  scoped_file_size_limit &operator=(const scoped_file_size_limit &) = delete;

  bool active() const { return m_limit_changed; }

 private:
  struct sigaction m_old_action{};
  struct rlimit m_old_limit{};
  bool m_action_changed{false};
  bool m_limit_changed{false};
};

TEST(FileCopyTest, CopyFileReportsWriteError) {
  const auto dir = prepare_test_dir("write_error");
  const auto source = dir / "source";
  const auto destination = dir / "destination";

  constexpr std::size_t source_size = 20 << 20;
  constexpr rlim_t max_file_size = 8 << 20;
  ASSERT_TRUE(write_file(source, make_content(source_size)));

  for (const auto &mode : {k_sparse, k_dense}) {
    SCOPED_TRACE(mode.name);
    ASSERT_TRUE(mdtl::remove_file(destination));

    bool copied = false;
    {
      // A write to the destination past max_file_size fails with EFBIG.
      const scoped_file_size_limit limit(max_file_size);
      ASSERT_TRUE(limit.active());
      copied = mdtl::copy_file(source, destination, mode.flags);
    }
    const ssize_t size = mdtl::get_file_size(destination);

    EXPECT_FALSE(copied) << "copy_file returned true, the destination has "
                         << size << " of " << source_size << " bytes";
    // A copy that reports success has all bytes of the source.
    if (copied) {
      EXPECT_EQ(size, ssize_t(source_size));
    }
  }
}

TEST(FileCopyTest, DenseCopyFailsWhenSourceEndsBeforeExpectedSize) {
#ifdef __linux__
  const auto dir = prepare_test_dir("short_source");
  const auto source = dir / "source";
  const auto destination = dir / "destination";
  constexpr std::size_t source_size = 1 << 20;
  ASSERT_TRUE(write_file(source, make_content(source_size)));

  // The source has fewer bytes than the expected size. This occurs when the
  // source becomes shorter after prepare_file_copy reads its size.
  constexpr std::size_t expected_size = source_size + 4096;
  const int src = ::open(source.c_str(), O_RDONLY);
  ASSERT_NE(src, -1) << "open: " << std::strerror(errno);
  const int dst = ::open(destination.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                         S_IRUSR | S_IWUSR);
  if (dst == -1) {
    ADD_FAILURE() << "open: " << std::strerror(errno);
    ::close(src);
    return;
  }
  const bool copied =
      mdtl::fcpdtl::copy_file_dense_linux(src, dst, expected_size);
  ::close(src);
  ::close(dst);

  EXPECT_FALSE(copied) << "copy_file_dense_linux returned true for "
                       << expected_size << " expected bytes. The source has "
                       << source_size << " bytes, and the destination has "
                       << mdtl::get_file_size(destination) << " bytes.";
#else
  GTEST_SKIP() << "copy_file_dense_linux exists only on Linux";
#endif
}

#ifdef __linux__
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
#endif

TEST(FileCopyTest, SparseCopyAcrossFileSystemsKeepsHoles) {
#ifdef __linux__
  // /dev/shm is a tmpfs. The test directory is on a different file system.
  const fs::path shm_dir("/dev/shm");
  struct stat shm_stat{};
  if (::stat(shm_dir.c_str(), &shm_stat) != 0 || !S_ISDIR(shm_stat.st_mode)) {
    GTEST_SKIP() << shm_dir << " is not a directory";
  }
  const auto dir = prepare_test_dir("sparse_across_file_systems");
  struct stat dir_stat{};
  ASSERT_EQ(::stat(dir.c_str(), &dir_stat), 0) << std::strerror(errno);
  if (dir_stat.st_dev == shm_stat.st_dev) {
    GTEST_SKIP() << dir << " and " << shm_dir << " are on one file system";
  }

  const auto source =
      shm_dir / test_utility::make_test_path("source").filename();
  const auto destination = dir / "destination";

  // Removes the two files at the end of the test, also after a failure.
  struct file_remover {
    fs::path path;
    ~file_remover() {
      std::error_code ec;
      fs::remove(path, ec);
    }
  };
  const file_remover source_remover{source};
  const file_remover destination_remover{destination};

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
    GTEST_SKIP() << "The source in " << shm_dir << " is not sparse";
  }

  const bool copied =
      mdtl::copy_file(source, destination, mdtl::copy_flags::sparse);
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
#else
  GTEST_SKIP() << "This test copies from the tmpfs /dev/shm of Linux";
#endif
}

}  // namespace
