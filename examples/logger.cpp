// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/copperr/copperr.hpp>

// define logger ops
extern "C" void copperr_log([[maybe_unused]] copperr_log_level lvl,
                            [[maybe_unused]] char const *file,
                            [[maybe_unused]] size_t line,
                            char const *function, char const *message) {
  std::cerr << "(" << function << "): " << message << std::endl;
}

int main() {
  // Do Copperr operations
  dice::copperr::manager manager(dice::copperr::create_only, "/tmp/dir");

  return 0;
}