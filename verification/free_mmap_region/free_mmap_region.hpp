// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_FREE_MEMORY_SPACE_HPP
#define METALL_FREE_MEMORY_SPACE_HPP

#include <iostream>
#include <string>
#include <cassert>
#include <functional>
#include <thread>

#include <dice/copperr/detail/file.hpp>
#include <dice/copperr/detail/mmap.hpp>
#include <dice/copperr/detail/time.hpp>
#include <dice/copperr/detail/memory.hpp>
#include <dice/copperr/detail/utilities.hpp>

namespace mdtl = dice::copperr::mtlldetail;

static constexpr int k_map_nosync =
#ifdef MAP_NOSYNC
    MAP_NOSYNC;
#else
    0;
#endif

std::size_t get_page_size() {
  const auto page_size = mdtl::get_page_size();
  if (page_size <= 0) {
    std::cerr << __LINE__ << " Failed to get the page size" << std::endl;
    std::abort();
  }
  return static_cast<size_t>(page_size);
}

std::pair<int, void *> map_file_share(const std::string &file_path,
                                      const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();

  std::cout << "Map size: " << size << std::endl;

  if (!mdtl::create_file(file_path) ||
      !mdtl::extend_file_size(file_path, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_path
              << std::endl;
    std::abort();
  }

  const auto ret =
      mdtl::map_file_write_mode(file_path, nullptr, size, 0, k_map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return ret;
}

std::pair<int, void *> map_file_private(const std::string &file_path,
                                        const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();

  std::cout << "Map size: " << size << std::endl;

  if (!mdtl::create_file(file_path) ||
      !mdtl::extend_file_size(file_path, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_path
              << std::endl;
    std::abort();
  }

  const auto ret = mdtl::map_file_write_private_mode(file_path, nullptr, size,
                                                     0, k_map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return ret;
}

void unmap(void *const addr, const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();

  if (!mdtl::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void sync_mmap(void *const addr, const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();
  mdtl::os_msync(addr, size, true);
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void sync_file(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }

  const auto start = mdtl::elapsed_time_sec();
  mdtl::os_fsync(fd);
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void close_file(const int fd) {
  const auto start = mdtl::elapsed_time_sec();
  if (!mdtl::os_close(fd)) {
    std::cerr << __LINE__ << " Failed to close file" << std::endl;
    std::abort();
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

#endif  // METALL_FREE_MEMORY_SPACE_HPP
