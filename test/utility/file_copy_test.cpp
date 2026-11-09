// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

}  // namespace
