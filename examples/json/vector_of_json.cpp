// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <dice/metall/metall.hpp>
#include <dice/metall/container/vector.hpp>
#include <dice/metall/json/json.hpp>

namespace mc = dice::metall::container;
namespace mj = dice::metall::json;

int main() {
  // Metall JSON container works like the other containers w.r.t. allocation,
  // i.e., it takes an allocator type in its template parameter and an allocator
  // object in its constructors.
  using json_value_type = mj::value<dice::metall::manager::allocator_type<std::byte>>;
  // Need to use scoped_allocator as this is a multi-layer container.
  using vector_json_type =
      mc::vector<json_value_type,
                 dice::metall::manager::scoped_allocator_type<json_value_type>>;

  // An example input json strings.
  std::vector<std::string> json_string_list{
      R"({"name": "Alice", "list": [0, 1]})",
      R"({"name": "Brad", "list": [2, 3]})"};

  // Create a vector-of-json object
  {
    dice::metall::manager manager(dice::metall::create_only, "./test");
    auto *vec = manager.construct<vector_json_type>(dice::metall::unique_instance)(
        manager.get_allocator());
    for (const auto &json_string : json_string_list) {
      vec->emplace_back(mj::parse(json_string, manager.get_allocator()));
    }
  }

  // Reattach the vector-of-json object created above.
  {
    dice::metall::manager manager(dice::metall::open_read_only, "./test");
    auto *vec = manager.find<vector_json_type>(dice::metall::unique_instance).first;
    for (const auto &json : *vec) {
      mj::pretty_print(std::cout, json);  // Show contents.
    }
  }

  return 0;
}
