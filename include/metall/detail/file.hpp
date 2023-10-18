// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_FILE_HPP
#define METALL_DETAIL_UTILITY_FILE_HPP

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

#include <metall/logger.hpp>

namespace metall::mtlldetail {

inline bool os_close(const int fd) {
  if (::close(fd) == -1) {
    METALL_ERRNO_WARN("close");
    return false;
  }
  return true;
}

inline bool os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    METALL_ERRNO_WARN("fsync");
    return false;
  }
  return true;
}

inline bool fsync(std::filesystem::path const &path) {
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

inline bool fsync_recursive(std::filesystem::path const &path) {
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

inline bool extend_file_size(std::filesystem::path const &file_path,
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
inline bool file_exist(std::filesystem::path const &file_name) {
  return (::access(file_name.c_str(), F_OK) == 0);
}

/// \brief Check if a directory exists
/// \warning This implementation could return a wrong result due to metadata
/// cache on NFS.
inline bool directory_exist(std::filesystem::path const &dir_path) {
  struct stat stat_buf;
  if (::stat(dir_path.c_str(), &stat_buf) == -1) {
    return false;
  }
  return S_ISDIR(stat_buf.st_mode);
}

inline bool create_file(std::filesystem::path const &file_path) {
  const int fd =
      ::open(file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    METALL_ERRNO_ERROR("open");
    return false;
  }

  if (!os_close(fd)) return false;

  return fsync_recursive(file_path);
}

inline bool create_directory(std::filesystem::path const &dir_path) {
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

inline ssize_t get_file_size(std::filesystem::path const &file_path) {
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
inline ssize_t get_actual_file_size(std::filesystem::path const &file_path) {
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
inline bool remove_file(std::filesystem::path const &path) {
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

inline bool copy_file_dense(std::filesystem::path const &source_path,
                            std::filesystem::path const &destination_path) {
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
inline bool prepare_file_copy_linux(std::filesystem::path const &source_path,
                                    std::filesystem::path const &destination_path,
                                    int *src,
                                    int *dst) {
  *src = ::open(source_path.c_str(), O_RDONLY);
  if (*src == -1) {
    METALL_ERRNO_ERROR("Unable to open {}", source_path.c_str());
    return false;
  }

  struct stat st;
  if (::fstat(*src, &st) == -1) {
    METALL_ERRNO_ERROR("Unable to stat {}", source_path.c_str());
    os_close(*src);
    return false;
  }

  *dst = ::open(destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  if (*dst == -1) {
    METALL_ERRNO_ERROR("Unable to open {}", destination_path.c_str());
    os_close(*src);
    return false;
  }

  if (::fchmod(*dst, st.st_mode) == -1) {
    METALL_ERRNO_ERROR("Unable to chmod {}", destination_path.c_str());
    os_close(*src);
    os_close(*dst);
    return false;
  }

  return true;
}

inline bool copy_file_sparse_linux(int src, int dst) {
  off64_t off = 0;
  while ((off = ::lseek64(src, off, SEEK_DATA)) != -1) {
    off64_t const hole_start = ::lseek64(src, off, SEEK_HOLE);
    assert(hole_start != -1); // cannot fail, always an implicit hole at the end of the file

    if (::copy_file_range(src, &off, dst, NULL, hole_start - off, 0) == -1) {
      METALL_ERRNO_ERROR("copy_file_range");
      return false;
    }

    off = hole_start;
  }

  os_fsync(dst);
  return true;
}

inline bool copy_file_sparse_linux(std::filesystem::path const &source_path,
                                   std::filesystem::path const &destination_path) {
  int src;
  int dst;
  if (!prepare_file_copy_linux(source_path, destination_path, &src, &dst)) {
    METALL_ERROR("Unable to prepare for file copy");
    return false;
  }

  if (copy_file_sparse_linux(src, dst)) {
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
#endif  // __linux__
}  // namespace file_copy_detail

/// \brief Copy a file.
/// \param source_path A source file path.
/// \param destination_path A destination path.
/// \param sparse_copy If true is specified, tries to perform sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline bool copy_file(std::filesystem::path const &source_path,
                      std::filesystem::path const &destination_path,
                      const bool sparse_copy = true) {
  if (sparse_copy) {
#ifdef __linux__
    return file_copy_detail::copy_file_sparse_linux(source_path,
                                                    destination_path);
#else
    METALL_WARN("Sparse file copy is only supported on linux, falling back to normal copy");
#endif
  }

  return file_copy_detail::copy_file_dense(source_path, destination_path);
}

/// \brief Get the file names in a directory.
/// This function does not list files recursively.
/// Only regular files are returned.
/// \param dir_path A directory path.
/// \param file_list A buffer to put results.
/// \return Returns true if there is no error (empty directory returns true as
/// long as the operation does not fail). Returns false on error.
inline bool get_regular_file_names(std::filesystem::path const &dir_path,
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
    std::filesystem::path const &source_dir_path, std::filesystem::path const &destination_dir_path,
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

  const auto num_threads = std::min(
      src_file_names.size(),
      (std::size_t)(max_num_threads > 0 ? max_num_threads
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
    std::filesystem::path const &source_dir_path, std::filesystem::path const &destination_dir_path,
    const int max_num_threads, const bool sparse_copy = true) {
  return copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads,
      [sparse_copy](auto const &src, auto const &dst) {
        return copy_file(src, dst, sparse_copy);
      });
}
}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_HPP
