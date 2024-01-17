// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <boost/container/string.hpp>
#include <dice/copperr/copperr.hpp>
#include <string>

// String with Metall
using persistent_string =
    boost::container::basic_string<char, std::char_traits<char>,
                                   dice::copperr::manager::allocator_type<char>>;

int main() {
  {
    dice::copperr::manager manager(dice::copperr::create_only, "/tmp/datastore");
    auto pstr = manager.construct<persistent_string>("mystring")(
        "Hello, World!", manager.get_allocator<>());
    std::cout << *pstr << std::endl;
  }

  {
    dice::copperr::manager manager(dice::copperr::open_only, "/tmp/datastore");
    auto pstr = manager.find<persistent_string>("mystring").first;
    std::cout << *pstr << std::endl;
  }

  return 0;
}