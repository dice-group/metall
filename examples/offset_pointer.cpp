// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This is an example shows how to store pointers in persistent memory using the
// offset pointer

#include <iostream>
#include <dice/metall/metall.hpp>

// dice::copperr::offset_ptr is just an alias of offset_ptr in Boost.Interprocess
// https://www.boost.org/doc/libs/release/doc/html/interprocess/offset_ptr.html
using int_offset_prt = dice::copperr::offset_ptr<int>;

int main() {
  {
    dice::copperr::manager manager(dice::copperr::create_only, "/tmp/datastore");

    // Allocate a simple array in persistent memory
    int *array = static_cast<int *>(manager.allocate(10 * sizeof(int)));
    array[0] = 1;
    array[1] = 2;

    // Allocate an offset pointer with key 'ptr' and initialize its value with
    // the address of 'array'
    [[maybe_unused]] int_offset_prt *ptr =
        manager.construct<int_offset_prt>("ptr")(array);
  }

  {
    dice::copperr::manager manager(dice::copperr::open_only, "/tmp/datastore");

    int_offset_prt *ptr = manager.find<int_offset_prt>("ptr").first;

    // dice::copperr::to_raw_pointer extracts a raw pointer from dice::copperr::offset_ptr
    // If a raw pointer is given, it just returns the address the given pointer
    // points to
    int *array = dice::copperr::to_raw_pointer(*ptr);

    std::cout << array[0] << std::endl;  // Print 1
    std::cout << array[1] << std::endl;  // Print 2

    // Deallocate the array
    manager.deallocate(dice::copperr::to_raw_pointer(*ptr));
    *ptr = nullptr;

    // Destroy the offset pointer object
    manager.destroy<int_offset_prt>("ptr");
  }

  return 0;
}