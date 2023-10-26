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

#include <cstdlib>
#include <metall/detail/file.hpp>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

namespace file_clone_detail {
#ifdef __linux__

/**
 * Clone file using
 * https://man7.org/linux/man-pages/man2/ioctl_ficlone.2.html
 */
inline bool clone_file_linux(int src, int dst) {
#ifdef FICLONE
  return ::ioctl(dst, FICLONE, src) != -1;
#else
  return false;
#endif  // defined(FICLONE)
}

inline bool clone_file_linux(const std::filesystem::path &source_path,
                             const std::filesystem::path &destination_path) {
  int src;
  int dst;
  if (!file_copy_detail::prepare_file_copy_linux(source_path, destination_path, &src, &dst)) {
    METALL_ERROR("Unable to prepare for file copy");
    return false;
  }

  auto close_fsync_all = [&]() noexcept {
    os_fsync(dst);
    os_close(src);
    os_close(dst);
  };

  if (clone_file_linux(src, dst)) {
    close_fsync_all();
    return true;
  }

  METALL_WARN("Unable to clone {} to {}, falling back to sparse copy", source_path.c_str(), destination_path.c_str());

  if (file_copy_detail::copy_file_sparse_linux(src, dst)) {
    close_fsync_all();
    return true;
  }

  METALL_WARN("Unable to sparse copy {} to {}, falling back to normal copy", source_path.c_str(), destination_path.c_str());
  os_close(src);
  os_close(dst);

  if (file_copy_detail::copy_file_dense(source_path, destination_path)) {
    return true;
  }

  METALL_ERROR("Unable to copy {} to {}", source_path.c_str(), destination_path.c_str());
  return false;
}
#endif  // __linux__
}  // namespace file_clone_detail

/// \brief Clones a file. If file cloning is not supported, copies the file
/// normally. \param source_path A path to the file to be cloned. \param
/// destination_path A path to copy to. \return On success, returns true. On
/// error, returns false.
inline bool clone_file(const std::filesystem::path &source_path,
                       const std::filesystem::path &destination_path) {
#if defined(__linux__)
  return file_clone_detail::clone_file_linux(source_path, destination_path);
#else
  METALL_WARN("Cloning is only supported on linux, falling back to copying");
  return mtlldetail::copy_file(source_path, destination_path);
#endif
}

/// \brief Clone files in a directory.
/// This function does not clone files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \return  On success, returns true. On error, returns false.
inline bool clone_files_in_directory_in_parallel(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const int max_num_threads) {
  return copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads, clone_file);
}

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_CLONE_HPP
