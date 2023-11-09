// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/metall/metall.hpp>

// define logger ops
extern "C" void metall_log([[maybe_unused]] metall_log_level lvl,
                           [[maybe_unused]] char const *file,
                           [[maybe_unused]] size_t line,
                           char const *function, char const *message) {
  std::cerr << "(" << function << "): " << message << std::endl;
}

int main() {
  // Do Metall operations
  dice::metall::manager manager(dice::metall::create_only, "/tmp/dir");

  return 0;
}