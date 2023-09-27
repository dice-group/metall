// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_FILE_CLONE_HPP
#define METALL_DETAIL_UTILITY_FILE_CLONE_HPP

#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/fs.h>
#endif

#ifdef __APPLE__
#include <sys/attr.h>
#include <sys/clonefile.h>
#endif

#include <cstdlib>
#include <metall/detail/file.hpp>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

namespace file_clone_detail {
#ifdef __linux__
inline void clone_file_linux(const std::filesystem::path &source_path,
                             const std::filesystem::path &destination_path) {
  std::ostringstream oss;
  oss << "cp --reflink=auto -R " << source_path << ' ' << destination_path;

  auto command = oss.str();
  const int status = std::system(command.c_str());

  if (status == -1 || !WIFEXITED(status)) {
    throw std::runtime_error{"Reflink copy failed"};
  }
}
#endif
}  // namespace file_clone_detail

/// \brief Clones a file. If file cloning is not supported, copies the file
/// normally. \param source_path A path to the file to be cloned. \param
/// destination_path A path to copy to. \return On success, returns true. On
/// error, returns false.
inline void clone_file(const std::filesystem::path &source_path,
                       const std::filesystem::path &destination_path) {
  file_clone_detail::clone_file_linux(source_path, destination_path);
  metall::mtlldetail::fsync(destination_path);
}

/// \brief Clone files in a directory.
/// This function does not clone files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \return  On success, returns true. On error, returns false.
inline void clone_files_in_directory_in_parallel(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const int max_num_threads) {
  copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads, clone_file);
}

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_CLONE_HPP
