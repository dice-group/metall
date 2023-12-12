// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_FILE_HPP
#define METALL_DETAIL_UTILITY_FILE_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>

#ifdef __linux__
#include <linux/falloc.h>  // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>

#include <dice/metall/logger.hpp>

namespace dice::metall::mtlldetail {

/**
 * Calls ::close, logs a warning on error
 * @return true if ::close() succeeded, false otherwise
 */
inline bool os_close(const int fd) {
  if (::close(fd) == -1) {
    METALL_ERRNO_WARN("close");
    return false;
  }
  return true;
}

/**
 * Calls ::fsync, logs a warning on error
 * @return true if ::fsync() succeeded, false otherwise
 */
inline bool os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    METALL_ERRNO_WARN("fsync");
    return false;
  }
  return true;
}

inline bool fsync(const std::filesystem::path &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    METALL_ERRNO_ERROR("open");
    return false;
  }

  bool ret = true;
  ret &= os_fsync(fd);
  ret &= os_close(fd);

  return ret;
}

inline bool fsync_recursive(const std::filesystem::path &path) {
  auto p = std::filesystem::canonical(path);
  while (true) {
    if (!fsync(p.string())) {
      return false;
    }
    if (p == p.root_path()) {
      break;
    }
    p = p.parent_path();
  }
  return true;
}

inline bool extend_file_size_manually(const int fd, const off_t offset,
                                      const ssize_t file_size) {
  auto buffer = new unsigned char[4096];
  for (off_t i = offset; i < file_size / 4096 + offset; ++i) {
    ::pwrite(fd, buffer, 4096, i * 4096);
  }
  const size_t remained_size = file_size % 4096;
  if (remained_size > 0)
    ::pwrite(fd, buffer, remained_size, file_size - remained_size);

  delete[] buffer;

  const bool ret = os_fsync(fd);

  return ret;
}

inline bool extend_file_size(const int fd, const size_t file_size,
                             const bool fill_with_zero) {
  if (fill_with_zero) {
#ifdef __APPLE__
    if (!extend_file_size_manually(fd, 0, file_size)) {
      METALL_ERROR("Failed to extend file size manually, filling zero");
      return false;
    }
#else
    if (::posix_fallocate(fd, 0, file_size) == -1) {
      METALL_ERRNO_ERROR("fallocate");
      return false;
    }
#endif
  } else {
    // -----  extend the file if its size is smaller than that of mapped area
    // ----- //
    struct stat stat_buf;
    if (::fstat(fd, &stat_buf) == -1) {
      METALL_ERRNO_ERROR("fstat");
      return false;
    }
    if (::llabs(stat_buf.st_size) < static_cast<ssize_t>(file_size)) {
      if (::ftruncate(fd, file_size) == -1) {
        METALL_ERRNO_ERROR("ftruncate");
        return false;
      }
    }
  }

  const bool ret = os_fsync(fd);
  return ret;
}

inline bool extend_file_size(const std::filesystem::path &file_path,
                             const size_t file_size,
                             const bool fill_with_zero = false) {
  const int fd = ::open(file_path.c_str(), O_RDWR);
  if (fd == -1) {
    METALL_ERRNO_ERROR("open");
    return false;
  }

  bool ret = extend_file_size(fd, file_size, fill_with_zero);
  ret &= os_close(fd);

  return ret;
}

/// \brief Check if a file, any kinds of file including directory, exists
/// \warning This implementation could return a wrong result due to metadata
/// cache on NFS. The following code could fail: if (mpi_rank == 1)
/// file_exist(path); // NFS creates metadata cache mpi_barrier(); if (mpi_rank
/// == 0) create_directory(path); mpi_barrier(); if (mpi_rank == 1)
/// assert(file_exist(path)); // Could fail due to the cached metadata.
inline bool file_exist(const std::filesystem::path &file_name) {
  return (::access(file_name.c_str(), F_OK) == 0);
}

/// \brief Check if a directory exists
/// \warning This implementation could return a wrong result due to metadata
/// cache on NFS.
inline bool directory_exist(const std::filesystem::path &dir_path) {
  struct stat stat_buf;
  if (::stat(dir_path.c_str(), &stat_buf) == -1) {
    return false;
  }
  return S_ISDIR(stat_buf.st_mode);
}

inline bool create_file(const std::filesystem::path &file_path) {
  const int fd =
      ::open(file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    METALL_ERRNO_ERROR("open");
    return false;
  }

  if (!os_close(fd)) return false;

  return fsync_recursive(file_path);
}

inline bool create_directory(const std::filesystem::path &dir_path) {
  bool success = true;
  try {
    std::error_code ec;
    if (!std::filesystem::create_directories(dir_path, ec)) {
      if (!ec) {
        // if the directory exist, create_directories returns false.
        // However, std::error_code is cleared and !ec returns true.
        return true;
      }

      METALL_ERROR("{}", ec.message());
      success = false;
    }
  } catch (std::filesystem::filesystem_error &e) {
    METALL_ERROR("{}", e.what());
    success = false;
  }

  return success;
}

inline ssize_t get_file_size(const std::filesystem::path &file_path) {
  std::ifstream ifs(file_path, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    METALL_ERROR("Failed to get file size: {}", file_path.c_str());
  }

  return size;
}

/// \brief
/// Note that, according to GCC,
/// the file system may use some blocks for internal record keeping
inline ssize_t get_actual_file_size(const std::filesystem::path &file_path) {
  struct stat stat_buf;
  if (::stat(file_path.c_str(), &stat_buf) != 0) {
    METALL_ERRNO_ERROR("stat ({})", file_path.c_str());
    return -1;
  }
  return stat_buf.st_blocks * 512LL;
}

/// \brief Remove a file or directory
/// \return Upon successful completion, returns true; otherwise, false is
/// returned. If the file or directory does not exist, true is returned.
inline bool remove_file(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
  return !ec;
}

inline bool free_file_space([[maybe_unused]] const int fd,
                            [[maybe_unused]] const off_t off,
                            [[maybe_unused]] const off_t len) {
#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len) ==
      -1) {
    METALL_ERRNO_WARN("fallocate");
    return false;
  }
  return true;

#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "FALLOC_FL_PUNCH_HOLE or FALLOC_FL_KEEP_SIZE is not supported"
#endif
  return false;
#endif
}

namespace file_copy_detail {

inline bool copy_file_dense(const std::filesystem::path &source_path,
                            const std::filesystem::path &destination_path) {
  bool success = true;
  try {
    if (!std::filesystem::copy_file(source_path, destination_path,
                       std::filesystem::copy_options::overwrite_existing)) {
      METALL_ERROR("Failed copying file {} to {}", source_path.c_str(), destination_path.c_str());
      success = false;
                       }
  } catch (std::filesystem::filesystem_error &e) {
    METALL_ERROR("{}", e.what());
    success = false;
  }

  mtlldetail::fsync(destination_path);
  return success;
}

#ifdef __linux__
/**
 * @brief Prepares a file copy from source_path to destination_path
 *      by opening/creating the relevant files and setting appropriate permissions.
 *
 * @param source_path path to source file
 * @param destination_path desired path of destination file
 * @param src out parameter for the file descriptor opened for source_path (will be opened read only)
 * @param dst out parameter for the file descriptor opened for destination_path (will be opened write only)
 * @return on success: size of source file as obtained by ::fstat. on failure: -1
 *
 * @warning if the function fails the user must not use the obtained src and dst file descriptors in any way
 *      (they don't need to be closed)
 */
inline off_t prepare_file_copy_linux(const std::filesystem::path &source_path,
                                     const std::filesystem::path &destination_path,
                                     int *src,
                                     int *dst) {
  *src = ::open(source_path.c_str(), O_RDONLY);
  if (*src == -1) {
    METALL_ERRNO_ERROR("Unable to open {}", source_path.c_str());
    return -1;
  }

  struct stat st;
  if (::fstat(*src, &st) == -1) {
    METALL_ERRNO_ERROR("Unable to stat {}", source_path.c_str());
    os_close(*src);
    return -1;
  }

  *dst = ::open(destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
  if (*dst == -1) {
    METALL_ERRNO_ERROR("Unable to open {}", destination_path.c_str());
    os_close(*src);
    return -1;
  }

  return st.st_size;
}

/**
 * Creates a hole of size size at the current cursor of fd.
 * Moves cursor of fd behind the created hole.
 *
 * @param fd file descriptor
 * @param size size of to-be-created hole
 * @return if creation was successful
 * @note the cursor position will still be advanced even if hole punching fails
 *
 * Relevant man pages:
 *      https://www.man7.org/linux/man-pages/man2/lseek.2.html
 *      https://man7.org/linux/man-pages/man2/fallocate.2.html
 */
inline bool create_hole_linux(const int fd, const off_t size) {
  if (size == 0) {
    return true;
  }

  // Seek size bytes past the current position
  const off_t hole_end = ::lseek(fd, size, SEEK_CUR);
  if (hole_end < 0) {
    METALL_ERRNO_ERROR("lseek");
    return false;
  }

  // punch a hole from old cursor to new cursor
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, hole_end - size, size) < 0) {
    METALL_ERRNO_ERROR("fallocate(FALLOC_FL_PUNCH_HOLE)");
    return false;
  }

  return true;
}

/**
 * Performs a sparse copy from src to dst, by only copying actual data and manually recreating
 * all holes from src in dst.
 *
 * I.e. for each "segment" (data segment or hole) in src do
 *      if segment is data => copy segment to dst
 *      if segment is hole => create new hole (of the appropriate size) in dst
 *
 * @param src source file descriptor
 * @param dst destination file descriptor
 * @param src_size size of file behind src as obtained by ::fstat
 * @return if copying was successful
 *
 * Relevant man pages:
 *      https://www.man7.org/linux/man-pages/man2/lseek.2.html
 *      https://www.man7.org/linux/man-pages/man2/copy_file_range.2.html
 *      https://www.man7.org/linux/man-pages/man3/ftruncate.3p.html
 */
inline bool copy_file_sparse_linux(const int src, const int dst, const off_t src_size) {
  off_t old_off = 0;
  off_t off = 0;

  while ((off = ::lseek(src, off, SEEK_DATA)) >= 0) {
    if (!create_hole_linux(dst, off - old_off)) {
      METALL_ERROR("Unable to punch hole");
      return false;
    }

    off_t const hole_start = ::lseek(src, off, SEEK_HOLE);
    if (hole_start < 0) {
      METALL_ERRNO_ERROR("fseek(SEEK_HOLE)");
      return false;
    }

    if (::copy_file_range(src, &off, dst, nullptr, hole_start - off, 0) < 0) {
      METALL_ERRNO_ERROR("copy_file_range");
      return false;
    }

    old_off = off;
  }

  if (errno != ENXIO) {
    // error condition: offset is _not_ within a hole at the end of the file.
    // previous lseek from while-loop condition must have failed
    METALL_ERRNO_ERROR("fseek(SEEK_DATA)");
    return false;
  }

  if (old_off < src_size) {
    // the final extent is a hole we must call ftruncate
    // here in order to record the proper length in the destination.
    // See also: https://github.com/coreutils/coreutils/blob/a257b63ce7ebcc4577adb5406b39fc0edd61dcac/src/copy.c#L643-L658
    if (::ftruncate(dst, src_size) < 0) {
      METALL_ERRNO_ERROR("ftruncate");
      return false;
    }

    if (!create_hole_linux(dst, src_size - old_off)) {
      METALL_ERROR("Unable to punch hole");
      return false;
    }
  }

  return true;
}

/**
 * Attempts to perform a sparse copy from source_path to destination_path,
 * falling back to regular copy if the sparse copy fails.
 */
inline bool copy_file_sparse_linux(const std::filesystem::path &source_path,
                                   const std::filesystem::path &destination_path) {
  int src;
  int dst;
  const off_t src_size = prepare_file_copy_linux(source_path, destination_path, &src, &dst);
  if (src_size < 0) {
    METALL_ERROR("Unable to prepare for file copy");
    return false;
  }

  if (copy_file_sparse_linux(src, dst, src_size)) {
    os_fsync(dst);
    os_close(src);
    os_close(dst);
    return true;
  }

  METALL_WARN("Unable to sparse copy {} to {}, falling back to normal copy", source_path.c_str(), destination_path.c_str());
  os_close(src);
  os_close(dst);

  if (copy_file_dense(source_path, destination_path)) {
    return true;
  }

  METALL_ERROR("Unable to copy {} to {}", source_path.c_str(), destination_path.c_str());
  return false;
}

/**
 * @brief Performs an accelerated, in-kernel copy from src to dst
 * @param src source file descriptor
 * @param dst destination file descriptor
 * @param src_size size of source file as obtained by ::fstat(src)
 * @return if the operation was successful
 *
 * Relevant man pages:
 *    - https://www.man7.org/linux/man-pages/man2/copy_file_range.2.html
 */
inline bool copy_file_dense_linux(const int src, const int dst, const off_t src_size) {
  if (::copy_file_range(src, nullptr, dst, nullptr, src_size, 0) < 0) {
    METALL_ERRNO_ERROR("copy_file_range");
    return false;
  }

  return true;
}

/**
 * @brief performs a dense copy from source_path to destionation_path
 * @param source_path path to source file
 * @param destination_path path to destination file
 * @return if the operation was successful
 */
inline bool copy_file_dense_linux(const std::filesystem::path &source_path,
                                  const std::filesystem::path &destination_path) {
  int src;
  int dst;
  const off_t src_size = prepare_file_copy_linux(source_path, destination_path, &src, &dst);
  if (src_size >= 0) {
    if (copy_file_dense_linux(src, dst, src_size)) {
      os_fsync(dst);
      os_close(src);
      os_close(dst);
      return true;
    }
  }

  os_close(src);
  os_close(dst);
  METALL_WARN("Unable to use accelerated dense copy, falling back to unaccelerated dense copy");

  return copy_file_dense(source_path, destination_path);
}

#endif  // __linux__
}  // namespace file_copy_detail

/// \brief Copy a file.
/// \param source_path A source file path.
/// \param destination_path A destination path.
/// \param sparse_copy If true is specified, tries to perform sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline bool copy_file(const std::filesystem::path &source_path,
                      const std::filesystem::path &destination_path,
                      const bool sparse_copy = true) {
  if (sparse_copy) {
#ifdef __linux__
    return file_copy_detail::copy_file_sparse_linux(source_path, destination_path);
#else
    METALL_WARN("Sparse file copy is only supported on linux, falling back to normal copy");
#endif
  }

#ifdef __linux__
  return file_copy_detail::copy_file_dense_linux(source_path, destination_path);
#else
  return file_copy_detail::copy_file_dense(source_path, destination_path);
#endif
}

/// \brief Get the file names in a directory.
/// This function does not list files recursively.
/// Only regular files are returned.
/// \param dir_path A directory path.
/// \param file_list A buffer to put results.
/// \return Returns true if there is no error (empty directory returns true as
/// long as the operation does not fail). Returns false on error.
inline bool get_regular_file_names(const std::filesystem::path &dir_path,
                                   std::vector<std::string> *file_list) {
  if (!directory_exist(dir_path)) {
    return false;
  }

  try {
    file_list->clear();
    for (auto &p : std::filesystem::directory_iterator(dir_path)) {
      if (p.is_regular_file()) {
        file_list->push_back(p.path().filename().string());
      }
    }
  } catch (...) {
    METALL_ERROR("Exception was thrown");
    return false;
  }

  return true;
}

/// \brief Copy files in a directory.
/// This function does not copy files in subdirectories.
/// This function does not also copy directories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, the value is automatically determined.
/// \param copy_func The actual copy function.
/// \return  On success, returns true. On error, returns false.
template<typename F>
inline bool copy_files_in_directory_in_parallel_helper(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const int max_num_threads,
    F &&copy_func) {
  std::vector<std::string> src_file_names;
  if (!get_regular_file_names(source_dir_path, &src_file_names)) {
    METALL_ERROR("Failed to get file list");
    return false;
  }

  std::atomic_uint_fast64_t num_successes = 0;
  std::atomic_uint_fast64_t file_no_cnt = 0;
  auto copy_lambda = [&file_no_cnt, &num_successes, &source_dir_path,
                      &src_file_names, &destination_dir_path, &copy_func]() {
    while (true) {
      const auto file_no = file_no_cnt.fetch_add(1);
      if (file_no >= src_file_names.size()) break;
      std::filesystem::path const src_file_path = source_dir_path / src_file_names[file_no];
      std::filesystem::path const dst_file_path = destination_dir_path / src_file_names[file_no];
      num_successes.fetch_add(copy_func(src_file_path, dst_file_path) ? 1 : 0);
    }
  };

  const auto num_threads = std::min<size_t>(
      src_file_names.size(),
      (max_num_threads > 0 ? max_num_threads
                           : std::thread::hardware_concurrency()));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (size_t ix = 0; ix < num_threads; ++ix) {
    threads.emplace_back(copy_lambda);
  }

  for (auto &th : threads) {
    th.join();
  }

  return num_successes == src_file_names.size();
}

/// \brief Copy files in a directory.
/// This function does not copy files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \param sparse_copy Performs sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline bool copy_files_in_directory_in_parallel(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const int max_num_threads, const bool sparse_copy = true) {
  return copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads,
      [sparse_copy](auto const &src, auto const &dst) {
        return copy_file(src, dst, sparse_copy);
      });
}
}  // namespace dice::metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_HPP
