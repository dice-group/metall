// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <dice/metall/metall.hpp>

#include <dice/metall/detail/file.hpp>
#include "../test_utility.hpp"

#include <random>
#include <sys/mman.h>

namespace {

void create(const std::string &dir_path) {
  dice::metall::manager manager(dice::metall::create_only, dir_path);

  manager.construct<uint32_t>("a")(1);
  manager.construct<uint64_t>("b")(2);
}

void open(const std::string &dir_path) {
  dice::metall::manager manager(dice::metall::open_read_only, dir_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

const std::filesystem::path &original_dir_path() {
  const static std::filesystem::path path(test_utility::make_test_path("/original"));
  return path;
}

const std::filesystem::path &copy_dir_path() {
  const static std::filesystem::path path(test_utility::make_test_path("/copy"));
  return path;
}

TEST(CopyFileTest, SyncCopy) {
  dice::metall::manager::remove(original_dir_path());
  dice::metall::manager::remove(copy_dir_path());

  create(original_dir_path());

  ASSERT_TRUE(dice::metall::manager::copy(original_dir_path(),
                                    copy_dir_path()));

  open(copy_dir_path());
}

TEST(CopyFileTest, AsyncCopy) {
  dice::metall::manager::remove(original_dir_path());
  dice::metall::manager::remove(copy_dir_path());

  create(original_dir_path());

  auto handler = dice::metall::manager::copy_async(original_dir_path(),
                                             copy_dir_path());
  ASSERT_TRUE(handler.get());

  open(copy_dir_path());
}



constexpr size_t FILE_SIZE = 4096*4*11;
std::default_random_engine rng{std::random_device{}()};

// Utility functions

/**
 * Fills file at fd with FILE_SIZE random bytes
 */
void fill_file(int fd);

/**
 * Randomly punches 1-9 holes into fd.
 *
 * @param hole_at_start forces a hole to be created at the start of the file
 * @param hole_at_end forces a hole to be created at the end of the file
 */
void punch_holes(int fd, bool hole_at_start = false, bool hole_at_end = false);

/**
 * Convenience wrapper around ::mmap
 */
std::pair<char const *, off_t> mmap(int fd);

/**
 * Checks if files a and b are equal byte by byte
 */
void check_files_eq(int a, int b, std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b);

/**
 * Returns a list of all holes in the given file
 */
std::vector<std::pair<off_t, off_t>> get_holes(int fd);

/**
 * print all holes in fd
 */
 void list_holes(std::vector<std::pair<off_t, off_t>> const &holes);

/**
 * Checks if the holes in a and b are in the same places and of the same size
 */
void check_holes_eq(std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b);


TEST(CopyFileTest, CopyFileSparseLinux) {
  auto srcp = test_utility::make_test_path("copy_file_sparse-src.bin");
  auto dstp = test_utility::make_test_path("copy_file_sparse-dst.bin");
  auto dst2p = test_utility::make_test_path("copy_file_sparse-dst2.bin");

  std::uniform_int_distribution<size_t> b{0, 1};

  for (size_t ix = 0; ix < 1000; ++ix) {
    std::filesystem::remove(srcp);
    std::filesystem::remove(dstp);
    std::filesystem::remove(dst2p);

    {
      int src = ::open(srcp.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (src == -1) {
        perror("open");
        std::exit(1);
      }

      fill_file(src);
      punch_holes(src, b(rng), b(rng));
      close(src);
    }

    { // copy using copy_file_sparse_linux
      int src = ::open(srcp.c_str(), O_RDONLY);
      if (src == -1) {
        perror("open");
        std::exit(1);
      }

      struct stat st;
      fstat(src, &st);

      int dst = ::open(dstp.c_str(), O_CREAT | O_WRONLY | O_TRUNC, st.st_mode);
      if (dst == -1) {
        perror("open");
        std::exit(1);
      }

      ::dice::metall::mtlldetail::file_copy_detail::copy_file_sparse_linux(src, dst, st.st_size);
      close(src);
      close(dst);
    }

    { // copy using cp
      std::ostringstream srcoss;
      srcoss << srcp;

      std::ostringstream dstoss;
      dstoss << dst2p;

      auto cmd = std::format("cp --sparse=always {} {}", srcoss.str(), dstoss.str());
      int res = std::system(cmd.c_str());
      assert(WIFEXITED(res));
    }

    int src = ::open(srcp.c_str(), O_RDONLY);
    if (src == -1) {
      perror("open");
      std::exit(1);
    }

    int dst = ::open(dstp.c_str(), O_RDONLY);
    if (dst == -1) {
      perror("open");
      std::exit(1);
    }

    int dst2 = ::open(dst2p.c_str(), O_RDONLY);
    if (dst2 == -1) {
      perror("open");
      std::exit(1);
    }

    auto holes_src = get_holes(src);
    auto holes_dst = get_holes(dst);
    auto holes_dst2 = get_holes(dst2);

    std::cout << "src holes:\n";
    list_holes(holes_src);
    std::cout << std::endl;

    std::cout << "dst holes:\n";
    list_holes(holes_dst);
    std::cout << std::endl;

    std::cout << "dst2 holes:\n";
    list_holes(holes_dst2);
    std::cout << std::endl;

    std::cout << "comparing src, dst" << std::endl;
    check_files_eq(src, dst, holes_src, holes_dst);
    check_holes_eq(holes_src, holes_dst);

    std::cout << "comparing dst, dst2" << std::endl;
    check_files_eq(dst, dst2, holes_dst, holes_dst2);
    check_holes_eq(holes_dst, holes_dst2);

    std::cout << "comparing dst2, src" << std::endl;
    check_files_eq(dst2, src, holes_dst2, holes_src);
    check_holes_eq(holes_dst2, holes_src);
  }
}

void fill_file(int fd) {
  std::vector<unsigned char> buf;
  std::uniform_int_distribution<unsigned char> dist{1, std::numeric_limits<unsigned char>::max()};

  for (size_t ix = 0; ix < FILE_SIZE; ++ix) {
    buf.push_back(dist(rng));
  }

  size_t bytes_written = write(fd, buf.data(), buf.size());
  assert(bytes_written == FILE_SIZE);
}

void punch_holes(int fd, bool hole_at_start, bool hole_at_end) {
  std::uniform_int_distribution<size_t> hole_num_dist{static_cast<size_t>(1 + hole_at_start + hole_at_end), 10};
  std::uniform_int_distribution<off_t> hole_size_dist{1, FILE_SIZE/10};
  std::uniform_int_distribution<off_t> hole_off_dist{0, FILE_SIZE - FILE_SIZE/10};

  auto num_holes = hole_num_dist(rng);
  for (size_t ix = 0; ix < num_holes - hole_at_end - hole_at_end; ++ix) {
    auto hole_start = hole_off_dist(rng);
    auto hole_size = hole_size_dist(rng);

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, hole_start, hole_size) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }

  if (hole_at_start) {
    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, hole_size_dist(rng)) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }

  if (hole_at_end) {
    auto sz = hole_size_dist(rng);
    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 4096*4*6 - sz, sz) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }
}

std::pair<char const *, off_t> mmap(int fd) {
  struct stat st;
  int res = fstat(fd, &st);
  assert(res >= 0);

  auto *ptr = ::mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  assert(ptr != MAP_FAILED);

  return {static_cast<char const *>(ptr), st.st_size};
}

bool in_hole(off_t off, std::vector<std::pair<off_t, off_t>> const &holes) {
  return std::ranges::any_of(holes, [off](auto const &hole) {
    return off >= hole.first && off < hole.second;
  });
}

void check_files_eq(int a, int b, std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b) {
  auto am = mmap(a);
  auto a_ptr = am.first;
  auto a_size = am.second;

  auto bm = mmap(b);
  auto b_ptr = bm.first;
  auto b_size = bm.second;

  assert(a_size == b_size);

  for (size_t ix = 0; ix < static_cast<size_t>(a_size); ++ix) {
    if (in_hole(ix, holes_a)) {
      assert(a_ptr[ix] == 0);
    } else {
      assert(a_ptr[ix] != 0);
    }

    if (in_hole(ix, holes_b)) {
      assert(b_ptr[ix] == 0);
    } else {
      assert(b_ptr[ix] != 0);
    }

    if (a_ptr[ix] != b_ptr[ix]) {
      std::cerr << "found different bytes: " << std::hex << static_cast<int>(a_ptr[ix]) << " vs " << static_cast<int>(b_ptr[ix]) << std::endl;
      assert(false);
    }
  }
}

std::vector<std::pair<off_t, off_t>> get_holes(int fd) {
  lseek(fd, 0, SEEK_SET);
  std::vector<std::pair<off_t, off_t>> holes;

  off_t off = 0;
  off_t hole_start = 0;

  while (true) {
    hole_start = lseek(fd, off, SEEK_HOLE);
    off_t const hole_end = lseek(fd, hole_start, SEEK_DATA);

    if (hole_end == -1) {
      break;
    }

    off = hole_end;
    holes.emplace_back(hole_start, hole_end);
  }

  struct stat st;
  fstat(fd, &st);

  if (hole_start < st.st_size) {
    holes.emplace_back(hole_start, st.st_size);
  }

  return holes;
}

void list_holes(std::vector<std::pair<off_t, off_t>> const &holes) {
  for (auto const &hole : holes) {
    std::cout << "hole: " << hole.first << ".." << hole.second << std::endl;
  }
}

void check_holes_eq(std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b) {
  assert(holes_a.size() == holes_b.size());

  for (size_t ix = 0; ix < holes_a.size(); ++ix) {
    if (holes_a[ix] != holes_b[ix]) {
      std::cerr << "hole mismatch: " << holes_a[ix].first << ".." << holes_a[ix].second << " vs " << holes_b[ix].first << ".." << holes_b[ix].second << std::endl;
      assert(false);
    }
  }
}

}  // namespace
