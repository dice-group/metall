// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/metall/metall.hpp>

int main() {
  dice::metall::manager manager(dice::metall::create_only, "/tmp/dir");
  return 0;
}