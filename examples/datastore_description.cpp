// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cassert>

#include <dice/copperr/copperr.hpp>

int main() {
  // ----- Non-static methods ----- //
  {
    dice::copperr::manager manager(dice::copperr::create_only, "/tmp/dir");

    std::string buf;
    [[maybe_unused]] const auto ret = manager.get_description(&buf);
    assert(!ret && buf.empty());  // 'ret' is false and 'buf' is empty since
                                  // description has not been created yet.

    manager.set_description("description example");
    manager.get_description(&buf);
    std::cout << buf << std::endl;  // Shows "description example"
  }

  // ----- Static methods ----- //
  {
    dice::copperr::manager::set_description("/tmp/dir", "description example 2");

    std::string buf;
    dice::copperr::manager::get_description("/tmp/dir", &buf);
    std::cout << buf << std::endl;  // Shows "description example 2"
  }

  return 0;
}