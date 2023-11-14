// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to use STL containers with Metall

#include <iostream>
#include <dice/metall/metall.hpp>

// Metall contains basic STL containers that use metall as their default
// allocators.
#include <dice/metall/container/vector.hpp>

int main() {
  {
    dice::metall::manager manager(dice::metall::create_only, "/tmp/dir");

    // Allocate a vector object, passing an allocator object
    auto* vec = manager.construct<metall::container::vector<int>>("vec")(
        manager.get_allocator<int>());
    vec->resize(1);
    (*vec)[0] = 10;
    vec->push_back(20);
  }

  {
    dice::metall::manager manager(dice::metall::open_only, "/tmp/dir");

    auto* vec = manager.find<metall::container::vector<int>>("vec").first;
    std::cout << "Size = " << vec->size() << std::endl;
    std::cout << "Value at 0 = " << (*vec)[0] << std::endl;
    std::cout << "Value at 1 = " << (*vec)[1] << std::endl;

    vec->push_back(30);
  }

  {
    dice::metall::manager manager(dice::metall::open_read_only, "/tmp/dir");

    auto* vec = manager.find<metall::container::vector<int>>("vec").first;
    std::cout << "Value at 2 = " << (*vec)[2] << std::endl;
  }

  return 0;
}