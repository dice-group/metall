// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/logger.h>

using namespace metall;

void log_cerr() {
  logger::out(logger::level::silent, __FILE__, __LINE__, "silent logger");
  logger::out(logger::level::critical, __FILE__, __LINE__, "critical logger");
  METALL_ERROR("error logger");
  METALL_WARN("warning logger");
  METALL_INFO("info logger");
  METALL_DEBUG("debug logger");
  METALL_TRACE("verbose logger");
}

void log_perror() {
  logger::perror(logger::level::silent, __FILE__, __LINE__, "silent logger");
  logger::perror(logger::level::critical, __FILE__, __LINE__,
                 "critical logger");
  METALL_ERRNO_ERROR("error logger");
  METALL_ERRNO_WARN("warning logger");
  logger::perror(logger::level::info, __FILE__, __LINE__, "info logger");
  METALL_ERRNO_DEBUG("debug logger");
  METALL_ERRNO_TRACE("verbose logger");
}

int main() {
  logger::abort_on_critical_error(false);

  std::cerr << "--- Log level : unset ---" << std::endl;
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : silent ---" << std::endl;
  logger::set_log_level(logger::level::silent);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : critical ---" << std::endl;
  logger::set_log_level(logger::level::critical);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : error ---" << std::endl;
  logger::set_log_level(logger::level::error);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : warning ---" << std::endl;
  logger::set_log_level(logger::level::warning);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : info ---" << std::endl;
  logger::set_log_level(logger::level::info);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : debug ---" << std::endl;
  logger::set_log_level(logger::level::debug);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : verbose ---" << std::endl;
  logger::set_log_level(logger::level::verbose);
  log_cerr();
  log_perror();

  return 0;
}