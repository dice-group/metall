// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <dice/copperr/copperr.hpp>
#include <dice/copperr/json/json.hpp>

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

  using copperr_value_type =
      dice::copperr::json::value<dice::copperr::manager::allocator_type<std::byte>>;
  dice::copperr::manager manager(dice::copperr::create_only, "./test");

  auto *value = manager.construct<copperr_value_type>(dice::copperr::unique_instance)(
      dice::copperr::json::parse(json_string, manager.get_allocator()));
  dice::copperr::json::pretty_print(std::cout, *value);

  value->as_object()["name"].as_string() = "Alice";  // Change a string value

  value->as_object()["temperature"] = 25.2;  // Insert a double value
  value->as_object()["unit"] = "celsius";    // Insert a string value

  value->as_object().erase("pi");  // Erase a value

  auto pos = value->as_object().find("happy");
  std::cout << "Happy? : " << pos->value() << std::endl;

  dice::copperr::json::pretty_print(std::cout, *value);

  const auto clone(*value);
  std::cout << (clone == *value) << std::endl;

  return 0;
}
