// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to open existing Metall data store, checking whether
// it is consistent Please execute t5-2_create.cpp beforehand

#include <iostream>
#include <dice/metall/metall.hpp>

int main() {
  if (dice::copperr::manager::consistent("/tmp/dir")) {
    dice::copperr::manager manager(dice::copperr::open_read_only, "/tmp/dir");
    std::cout << "Opened /tmp/dir" << std::endl;
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;
  } else {
    // This block will be executed since "/tmp/dir" was not closed properly.
    std::cerr
        << "Inconsistent Metall data store --- /tmp/dir was not closed properly"
        << std::endl;
  }

  if (dice::copperr::manager::consistent("/tmp/snapshot")) {
    dice::copperr::manager manager(dice::copperr::open_read_only, "/tmp/snapshot");
    std::cout << "Opened /tmp/snapshot" << std::endl;
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;
  } else {
    std::cerr << "Inconsistent Metall data store --- /tmp/snapshot was not "
                 "closed properly"
              << std::endl;
  }

  return 0;
}