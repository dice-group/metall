// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to use multi-layer STL containers with Metall

#include <iostream>
#include <dice/metall/metall.hpp>
#include <dice/metall/container/vector.hpp>

// Vector of int
using inner_vector_t = dice::metall::container::vector<int>;

// Vector of vectors with scoped allocator adaptor
// In multi-level containers, one has to use scoped_allocator_adaptor in the
// most outer container so that the inner containers obtain their allocator
// arguments from the outer container's scoped_allocator_adaptor (see details:
// https://en.cppreference.com/w/cpp/memory/scoped_allocator_adaptor)
// dice::metall::manager has an allocator that is already wrapped with scoped
// allocator adaptor so that applications can use simple statements.
using outer_vector_type = dice::metall::container::vector<
    inner_vector_t, dice::metall::manager::scoped_allocator_type<inner_vector_t>>;

int main() {
  {
    dice::metall::manager manager(dice::metall::create_only, "/tmp/datastore");
    auto* vec = manager.construct<outer_vector_type>("vec-of-vecs")(
        manager.get_allocator());

    vec->resize(2);
    (*vec)[0].push_back(0);
    (*vec)[1].push_back(1);
  }

  {
    dice::metall::manager manager(dice::metall::open_only, "/tmp/datastore");
    auto* vec = manager.find<outer_vector_type>("vec-of-vecs").first;

    std::cout << (*vec)[0][0] << std::endl;  // Will print out 0
    std::cout << (*vec)[1][0] << std::endl;  // Will print out 1
  }

  return 0;
}