// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to allocate memory using Metall.

#include <iostream>
#include <dice/metall/metall.hpp>

int main() {
  dice::copperr::manager manager(dice::copperr::create_only, "/tmp/dir");

  // Allocate 'sizeof(int)' bytes, like malloc(sizeof(int))
  // The object is allocated into persistent memory;
  // however, one cannot reattached this object without storing its address.
  int *n = (int *)manager.allocate(sizeof(int));

  *n = 10;
  std::cout << *n << std::endl;

  manager.deallocate(n);  // Deallocate memory

  return 0;
}