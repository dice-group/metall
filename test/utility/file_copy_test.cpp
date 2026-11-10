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

#include <signal.h>
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

}  // namespace
