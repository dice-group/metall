// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_LOGGER_INTERFACE_H
#define METALL_LOGGER_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Log message level
typedef enum metall_log_level {
  /// \brief Error logger message
  METALL_LL_ERROR = 4,
  /// \brief Warning logger message
  METALL_LL_WARN = 3,
  /// \brief Info logger message
  METALL_LL_INFO = 2,
  /// \brief Debug logger message
  METALL_LL_DEBUG = 1,
  /// \brief Verbose (lowest priority) logger message
  METALL_LL_TRACE = 0,
} metall_log_level;

/// \brief Log a message
void metall_log(metall_log_level lvl, char const *function, char const *message);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // METALL_LOGGER_INTERFACE_H
