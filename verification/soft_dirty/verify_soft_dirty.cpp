// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/copperr/detail/soft_dirty_page.hpp>
#include <dice/copperr/detail/mmap.hpp>
#include <dice/copperr/detail/file.hpp>
#include <dice/copperr/detail/memory.hpp>

bool run_in_core_test(const ssize_t page_size, const std::size_t num_pages,
                      char *const map) {
  const uint64_t page_no_offset = reinterpret_cast<uint64_t>(map) / page_size;
  for (uint64_t i = 0; i < 2; ++i) {
    if (!dice::copperr::mtlldetail::reset_soft_dirty_bit()) {
      std::cerr << "Failed to reset soft dirty bit" << std::endl;
      return false;
    }

    {
      dice::copperr::mtlldetail::pagemap_reader pr;
      for (uint64_t p = 0; p < num_pages; ++p) {
        const auto pagemap = pr.at(page_no_offset + p);
        if (pagemap == dice::copperr::mtlldetail::pagemap_reader::error_value) {
          std::cerr << "Failed to read pagemap at " << p << std::endl;
          return false;
        }
        if (dice::copperr::mtlldetail::check_soft_dirty_page(pagemap)) {
          std::cerr << "Page is dirty at " << p << std::endl;
        }
      }
    }

    // Partially write data
    for (uint64_t p = 0; p < num_pages; ++p) {
      if (p % 2 == i % 2) {
        map[page_size * p] = 0;
      }
    }
    // dice::copperr::mtlldetail::os_msync(map, page_size * num_pages);

    {
      dice::copperr::mtlldetail::pagemap_reader pr;
      for (uint64_t p = 0; p < num_pages; ++p) {
        const auto pagemap = pr.at(page_no_offset + p);
        if (pagemap == dice::copperr::mtlldetail::pagemap_reader::error_value) {
          std::cerr << "Failed to read pagemap at " << p << std::endl;
          return false;
        }
        const bool desired_dirty_flag = (p % 2 == i % 2);
        if (dice::copperr::mtlldetail::check_soft_dirty_page(pagemap) !=
            desired_dirty_flag) {
          std::cerr << "Dirty flag must be " << desired_dirty_flag << " at "
                    << p << std::endl;
          return false;
        }
      }
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  if (!dice::copperr::mtlldetail::file_exist("/proc/self/pagemap")) {
    std::cerr << "Pagemap file does not exist" << std::endl;
    std::abort();
  }

  const ssize_t page_size = dice::copperr::mtlldetail::get_page_size();
  if (page_size <= 0) {
    std::abort();
  }

  const std::size_t k_num_pages = 1024;
  {
    auto map = static_cast<char *>(dice::copperr::mtlldetail::map_anonymous_write_mode(
        nullptr, page_size * k_num_pages));
    if (map) {
      run_in_core_test(page_size, k_num_pages, map);
      dice::copperr::mtlldetail::munmap(map, page_size * k_num_pages, false);
    } else {
      std::cerr << "Failed anonymous mapping" << std::endl;
    }
  }

  std::string file_name;
  if (argc == 2) {
    file_name = argv[1];
  } else {
    std::cerr << "Skip file backed mmap" << std::endl;
    std::abort();
  }

  {
    dice::copperr::mtlldetail::remove_file(file_name);
    dice::copperr::mtlldetail::create_file(file_name);
    dice::copperr::mtlldetail::extend_file_size(file_name, page_size * k_num_pages);

    auto map =
        static_cast<char *>(dice::copperr::mtlldetail::map_file_write_mode(
                                file_name, nullptr, page_size * k_num_pages, 0)
                                .second);
    if (map) {
      run_in_core_test(page_size, k_num_pages, map);
      dice::copperr::mtlldetail::munmap(map, page_size * k_num_pages, false);
    } else {
      std::cerr << "Failed file mapping" << std::endl;
    }
  }

  {
    dice::copperr::mtlldetail::remove_file(file_name);
    dice::copperr::mtlldetail::create_file(file_name);
    dice::copperr::mtlldetail::extend_file_size(file_name, page_size * k_num_pages);

    auto map =
        static_cast<char *>(dice::copperr::mtlldetail::map_file_write_private_mode(
                                file_name, nullptr, page_size * k_num_pages, 0)
                                .second);
    if (map) {
      run_in_core_test(page_size, k_num_pages, map);
      dice::copperr::mtlldetail::munmap(map, page_size * k_num_pages, false);
    } else {
      std::cerr << "Failed file mapping" << std::endl;
    }
  }

  return 0;
}