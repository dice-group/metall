// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <dice/metall/metall.hpp>

int main() {
  // Set and get object attributes via dice::metall::manager object
  {
    dice::metall::manager manager(dice::metall::create_only, "/tmp/dir");
    auto *obj = manager.construct<int>("obj")();

    std::string description;
    manager.get_instance_description(obj, &description);
    std::cout << "Name: " << manager.get_instance_name(obj)
              << ", Length: " << manager.get_instance_length(obj)
              << ", Description: " << description << std::endl;

    manager.set_instance_description(obj, "description example 1");
  }

  // Set and get object attributes via object attribute accessor object
  // Using object attribute accessor, one can access object attribute without
  // allocating a dice::metall::manager object (i.e., w/o memory mapping files).
  {
    auto accessor = dice::metall::manager::access_named_object_attribute("/tmp/dir");
    for (const auto &object : accessor) {
      std::cout << "Name: " << object.name() << ", Length: " << object.length()
                << ", Description: " << object.description() << std::endl;
    }

    auto itr = accessor.find("obj");
    accessor.set_description(itr, "description example 2");
    std::cout << "Name: " << itr->name() << ", Length: " << itr->length()
              << ", Description: " << itr->description() << std::endl;
  }

  return 0;
}