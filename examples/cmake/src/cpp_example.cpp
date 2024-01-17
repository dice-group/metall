// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/copperr/copperr.hpp>

int main() {
  dice::copperr::manager manager(dice::copperr::create_only, "/tmp/dir");
  return 0;
}