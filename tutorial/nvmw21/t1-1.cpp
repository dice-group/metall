// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program allocates an simple (int) object and reattaches it using Metall.

#include <iostream>
#include <dice/copperr/copperr.hpp>

int main() {
  // Creating data into persistent memory
  {
    dice::copperr::manager manager(dice::copperr::create_only, "/tmp/dir");

    int *n = manager.construct<int>  // Allocates an 'int' object
             ("name")  // Stores the allocated memory address with key "name"
             (10);     // Call a constructor of the object
  }

  // ----------------------------------------------------------------------------------------------------
  // Assume that this program exits here and the following code block is
  // executed as another run
  // ----------------------------------------------------------------------------------------------------

  // Reattaching the data
  {
    dice::copperr::manager manager(dice::copperr::open_only, "/tmp/dir");

    int *n = manager.find<int>("name").first;
    std::cout << *n << std::endl;

    manager.destroy_ptr(n);  // Deallocate memory
  }

  return 0;
}