// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <dice/metall/logger.hpp>

using namespace dice::copperr;

void log_cerr() {
  METALL_ERROR("error logger");
  METALL_WARN("warning logger");
  METALL_INFO("info logger");
  METALL_DEBUG("debug logger");
  METALL_TRACE("verbose logger");
}

void log_perror() {
  METALL_ERRNO_ERROR("error logger");
  METALL_ERRNO_WARN("warning logger");
  METALL_ERRNO_INFO("info logger");
  METALL_ERRNO_DEBUG("debug logger");
  METALL_ERRNO_TRACE("verbose logger");
}

int main() {
  log_cerr();
  log_perror();
}