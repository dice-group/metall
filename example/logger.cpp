// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>

// define logger ops
extern "C" void metall_log([[maybe_unused]] metall_log_level lvl, char const *function, char const *message) {
  std::cerr << "(" << function << "): " << message << std::endl;
}

int main() {
  // Do Metall operations
  metall::manager manager(metall::create_only, "/tmp/dir");

  return 0;
}