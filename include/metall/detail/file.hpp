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
#include <functional>
#include <filesystem>

namespace metall::mtlldetail {

namespace {
namespace fs = std::filesystem;
}

inline void os_close(const int fd) {
  if (::close(fd) == -1) {
    throw std::system_error{errno, std::system_category(), "close"};
  }
}

inline void os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    throw std::system_error{errno, std::system_category(), "fsync"};
  }
}

inline void fsync(const std::filesystem::path &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    throw std::system_error{errno, std::system_category(), "open"};
  }

  try {
    os_fsync(fd);
  } catch (std::system_error const &e) {
    os_close(fd);
    throw e;
  }

  os_close(fd);
}

inline void fsync_recursive(std::filesystem::path path) {
  path = fs::canonical(path);
  while (true) {
    fsync(path);

    if (path == path.root_path()) {
      break;
    }
    path = path.parent_path();
  }
}

inline void extend_file_size(const int fd, const size_t file_size,
                             const bool fill_with_zero) {
  if (fill_with_zero) {
    if (int err_no = ::posix_fallocate(fd, 0, static_cast<off_t>(file_size)); err_no != 0) {
      throw std::system_error{err_no, std::system_category(), "posix_fallocate"};
    }
  } else {
    // -----  extend the file if its size is smaller than that of mapped area
    // ----- //
    struct stat stat_buf;
    if (::fstat(fd, &stat_buf) == -1) {
      throw std::system_error{errno, std::system_category(), "fstat"};
    }
    if (::llabs(stat_buf.st_size) < static_cast<ssize_t>(file_size)) {
      if (::ftruncate(fd, static_cast<off_t>(file_size)) == -1) {
        throw std::system_error{errno, std::system_category(), "ftruncate"};
      }
    }
  }
}

inline void extend_file_size(const std::filesystem::path &file_path,
                             const size_t file_size,
                             const bool fill_with_zero = false) {
  const int fd = ::open(file_path.c_str(), O_RDWR);
  if (fd == -1) {
    throw std::system_error{errno, std::system_category(), "open"};
  }

  try {
    extend_file_size(fd, file_size, fill_with_zero);
  } catch (std::system_error const &e) {
    os_close(fd);
    throw e;
  }

  os_close(fd);
}

inline void create_file(const std::filesystem::path &file_path) {
  const int fd = ::open(file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    throw std::system_error{errno, std::system_category(), "open"};
  }

  os_close(fd);
  fsync_recursive(file_path);
}

/// \brief Creates directories recursively.
/// \return Returns true if the directory was created or already exists.
/// Otherwise, returns false.
inline void create_directory(const std::filesystem::path &dir_path) {
  std::error_code ec;
  if (!fs::create_directories(dir_path, ec)) {
    if (!ec) {
      // if the directory exist, create_directories returns false.
      // However, std::error_code is cleared and !ec returns true.
      return;
    }

    throw std::system_error{ec, "create directory"};
  }
}

inline size_t get_file_size(const std::filesystem::path &file_path) {
  std::ifstream ifs(file_path, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    throw std::system_error{errno, std::system_category(), "Failed to get file size"};
  }

  return static_cast<size_t>(size);
}

/// \brief
/// Note that, according to GCC,
/// the file system may use some blocks for internal record keeping
inline size_t get_actual_file_size(const std::filesystem::path &file_path) {
  struct stat stat_buf;
  if (::stat(file_path.c_str(), &stat_buf) != 0) {
    throw std::system_error{errno, std::system_category(), "stat"};
  }
  return stat_buf.st_blocks * 512LL;
}

/// \brief Remove a file or directory
/// \return Upon successful completion, returns true; otherwise, false is
/// returned. If the file or directory does not exist, true is returned.
inline void remove_file(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
  if (ec) {
    throw std::system_error{ec, "remove file"};
  }
}

inline void free_file_space([[maybe_unused]] const int fd,
                            [[maybe_unused]] const off_t off,
                            [[maybe_unused]] const off_t len) {
#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len) == -1) {
    throw std::system_error{errno, std::system_category(), "fallocate"};
  }

#elif defined(METALL_VERBOSE_SYSTEM_SUPPORT_WARNING)
#warning "FALLOC_FL_PUNCH_HOLE or FALLOC_FL_KEEP_SIZE is not supported"
#endif
}

namespace file_copy_detail {

inline void copy_file_dense(const std::filesystem::path &source_path,
                            const std::filesystem::path &destination_path) {
  try {
    fs::copy_file(source_path, destination_path,
                  fs::copy_options::overwrite_existing);
  } catch (fs::filesystem_error &e) {
    throw std::system_error{e.code(), e.what()};
  }

  metall::mtlldetail::fsync(destination_path);
}

#ifdef __linux__
inline void copy_file_sparse_linux(const std::filesystem::path &source_path,
                                   const std::filesystem::path &destination_path) {
  // TODO WTF is this function

  std::ostringstream oss;
  oss << "cp --sparse=auto " << source_path << ' ' << destination_path;
  auto command = oss.str();

  const int status = std::system(command.c_str());
  const bool success = (status != -1) && !!(WIFEXITED(status));
  if (!success) {
    throw std::runtime_error{"Failed sparse copying file"};
  }
}
#endif

}  // namespace file_copy_detail

/// \brief Copy a file.
/// \param source_path A source file path.
/// \param destination_path A destination path.
/// \param sparse_copy If true is specified, tries to perform sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline void copy_file(const std::filesystem::path &source_path,
                      const std::filesystem::path &destination_path,
                      const bool sparse_copy = true) {
  if (sparse_copy) {
#ifdef __linux__
    file_copy_detail::copy_file_sparse_linux(source_path,
                                             destination_path);
#else
    METALL_WARN("Sparse file copy is not available");
#endif
  }
  file_copy_detail::copy_file_dense(source_path, destination_path);
}

/// \brief Get the file names in a directory.
/// This function does not list files recursively.
/// Only regular files are returned.
/// \param dir_path A directory path.
/// \param file_list A buffer to put results.
/// \return Returns true if there is no error (empty directory returns true as
/// long as the operation does not fail). Returns false on error.
inline std::vector<std::filesystem::path> get_regular_file_names(const std::filesystem::path &dir_path) {
  if (!std::filesystem::exists(dir_path)) {
    return {};
  }

  std::vector<std::filesystem::path> ret;

  try {
    for (auto const &p : std::filesystem::directory_iterator(dir_path)) {
      if (p.is_regular_file()) {
        ret.push_back(p.path().filename());
      }
    }

    return ret;
  } catch (std::filesystem::filesystem_error const &e) {
    throw std::system_error{e.code(), e.what()};
  }
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
inline void copy_files_in_directory_in_parallel_helper(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const size_t max_num_threads,
    const std::function<void(const std::filesystem::path &, const std::filesystem::path &)> &copy_func) {

  auto const src_file_names = get_regular_file_names(source_dir_path);

  std::atomic_uint_fast64_t num_successes = 0;
  std::atomic_uint_fast64_t file_no_cnt = 0;

  auto copy_lambda = [&file_no_cnt, &num_successes, &source_dir_path,
                      &src_file_names, &destination_dir_path, &copy_func]() {
    while (true) {
      const auto file_no = file_no_cnt.fetch_add(1);

      if (file_no >= src_file_names.size())
        break;

      auto const src_file_path = source_dir_path / src_file_names[file_no];
      auto const dst_file_path = destination_dir_path / src_file_names[file_no];

      try {
        copy_func(src_file_path, dst_file_path);
        num_successes.fetch_add(1);
      } catch (...) {
        // ignore
      }
    }
  };

  const auto num_threads = std::min(src_file_names.size(),
                                    max_num_threads > 0 ? max_num_threads
                                                        : std::thread::hardware_concurrency());

  std::vector<std::thread> threads;
  for (size_t ix = 0; ix < num_threads; ++ix) {
    threads.emplace_back(copy_lambda);
  }

  for (auto &th : threads) {
    th.join();
  }

  if (num_successes != src_file_names.size()) {
    throw std::runtime_error{"Parallel file copy failed"};
  }
}

/// \brief Copy files in a directory.
/// This function does not copy files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \param sparse_copy Performs sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline void copy_files_in_directory_in_parallel(
    const std::filesystem::path &source_dir_path, const std::filesystem::path &destination_dir_path,
    const size_t max_num_threads, const bool sparse_copy = true) {

  copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads,
      [&sparse_copy](const std::filesystem::path &src, const std::filesystem::path &dst) {
        copy_file(src, dst, sparse_copy);
      });
}
}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_HPP
