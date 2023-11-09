// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <dice/metall/metall.hpp>
#include <dice/metall/json/json.hpp>

int main() {
  std::string json_string = R"(
      {
        "pi": 3.141,
        "happy": true,
        "name": "Niels",
        "nothing": null,
        "answer": {
          "everything": 42
        },
        "list": [1, 0, 2],
        "object": {
          "currency": "USD",
          "value": 42.99
        }
      }
    )";

  std::cout << "Create" << std::endl;

  using metall_value_type =
      dice::metall::json::value<dice::metall::manager::allocator_type<std::byte>>;
  dice::metall::manager manager(dice::metall::create_only, "./test");

  auto *value = manager.construct<metall_value_type>(dice::metall::unique_instance)(
      dice::metall::json::parse(json_string, manager.get_allocator()));
  dice::metall::json::pretty_print(std::cout, *value);

  value->as_object()["name"].as_string() = "Alice";  // Change a string value

  value->as_object()["temperature"] = 25.2;  // Insert a double value
  value->as_object()["unit"] = "celsius";    // Insert a string value

  value->as_object().erase("pi");  // Erase a value

  auto pos = value->as_object().find("happy");
  std::cout << "Happy? : " << pos->value() << std::endl;

  dice::metall::json::pretty_print(std::cout, *value);

  const auto clone(*value);
  std::cout << (clone == *value) << std::endl;

  return 0;
}
