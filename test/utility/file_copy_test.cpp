// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <metall/detail/file.hpp>
#include <metall/detail/file_clone.hpp>

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

using copy_function = std::function<bool(const fs::path &, const fs::path &)>;

TEST(FileCopyTest, CopyToSameFileKeepsData) {
  const auto dir = prepare_test_dir("same_file");
  const auto file = dir / "file";
  const auto hard_link = dir / "hard_link";
  const auto symbolic_link = dir / "symbolic_link";
  const std::string content = make_content(64 * 1024 + 123);

  const std::vector<std::pair<std::string, copy_function>> functions = {
      {"copy_file(sparse_copy=true)",
       [](const fs::path &src, const fs::path &dst) {
         return mdtl::copy_file(src, dst, true);
       }},
      {"copy_file(sparse_copy=false)",
       [](const fs::path &src, const fs::path &dst) {
         return mdtl::copy_file(src, dst, false);
       }},
      {"clone_file", [](const fs::path &src, const fs::path &dst) {
         return mdtl::clone_file(src, dst);
       }}};

  // Each destination is a name of the source file.
  const std::vector<std::pair<std::string, fs::path>> destinations = {
      {"the source path", file},
      {"a hard link to the source", hard_link},
      {"a symbolic link to the source", symbolic_link}};

  for (const auto &[function_name, copy_or_clone] : functions) {
    for (const auto &[destination_name, destination] : destinations) {
      SCOPED_TRACE(function_name + ", destination is " + destination_name);

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

      EXPECT_FALSE(copy_or_clone(file, destination));
      EXPECT_EQ(fs::file_size(file), content.size());
      EXPECT_TRUE(read_file(file) == content)
          << "The content of the source file changed";
    }
  }
}

/// \brief Limits the size of the files that this process writes.
/// A write past the limit fails with EFBIG and does not stop the process.
/// The destructor restores the previous limit and the previous SIGXFSZ action.
class scoped_file_size_limit {
 public:
  explicit scoped_file_size_limit(const rlim_t max_file_size) {
    struct sigaction ignore_action {};
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
  struct sigaction m_old_action {};
  struct rlimit m_old_limit {};
  bool m_action_changed{false};
  bool m_limit_changed{false};
};

TEST(FileCopyTest, CopyFileReportsWriteError) {
  const auto dir = prepare_test_dir("write_error");
  const auto source = dir / "source";
  const auto sparse_destination = dir / "sparse_destination";
  const auto dense_destination = dir / "dense_destination";

  constexpr std::size_t source_size = 20 << 20;
  constexpr rlim_t max_file_size = 8 << 20;
  ASSERT_TRUE(write_file(source, make_content(source_size)));

  bool sparse_copied = false;
  bool dense_copied = false;
  {
    // A write to a destination past max_file_size fails with EFBIG.
    const scoped_file_size_limit limit(max_file_size);
    ASSERT_TRUE(limit.active());
    sparse_copied = mdtl::copy_file(source, sparse_destination, true);
    dense_copied = mdtl::copy_file(source, dense_destination, false);
  }
  const ssize_t sparse_size = mdtl::get_file_size(sparse_destination);
  const ssize_t dense_size = mdtl::get_file_size(dense_destination);

  EXPECT_FALSE(sparse_copied)
      << "copy_file(sparse_copy=true) returned true, the destination has "
      << sparse_size << " of " << source_size << " bytes";
  EXPECT_FALSE(dense_copied)
      << "copy_file(sparse_copy=false) returned true, the destination has "
      << dense_size << " of " << source_size << " bytes";

  // A copy that reports success has all bytes of the source.
  if (sparse_copied) {
    EXPECT_EQ(sparse_size, ssize_t(source_size));
  }
  if (dense_copied) {
    EXPECT_EQ(dense_size, ssize_t(source_size));
  }
}

}  // namespace
