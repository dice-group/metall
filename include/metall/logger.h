// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_LOGGER_H
#define METALL_LOGGER_H

#include <stdint.h>

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
  METAL_LL_INFO = 2,
  /// \brief Debug logger message
  METAL_LL_DEBUG = 1,
  /// \brief Verbose (lowest priority) logger message
  METALL_LL_TRACE = 0,
} metall_log_level;

/// \brief Log a message
void metall_log(metall_log_level lvl, char const *file_name, uint64_t line, char const *message);

/// \brief Log a message about the current errno
void metall_errno_log(metall_log_level lvl, char const *file_name, uint64_t line, char const *message);


#ifdef __cplusplus
} // extern "C"
#endif

#define METALL_ERROR(msg) metall_log(METALL_LL_ERROR, __FILE__, __LINE__, msg)
#define METALL_WARN(msg) metall_log(METALL_LL_WARN, __FILE__, __LINE__, msg)
#define METALL_INFO(msg) metall_log(METAL_LL_INFO, __FILE__, __LINE__, msg)
#define METALL_DEBUG(msg) metall_log(METAL_LL_DEBUG, __FILE__, __LINE__, msg)
#define METALL_TRACE(msg) metall_log(METALL_LL_TRACE, __FILE__, __LINE__, msg)

#define METALL_ERRNO_ERROR(msg) metall_errno_log(METALL_LL_ERROR, __FILE__, __LINE__, msg)
#define METALL_ERRNO_WARN(msg) metall_errno_log(METALL_LL_WARN, __FILE__, __LINE__, msg)
#define METALL_ERRNO_INFO(msg) metall_errno_log(METAL_LL_INFO, __FILE__, __LINE__, msg)
#define METALL_ERRNO_DEBUG(msg) metall_errno_log(METAL_LL_DEBUG, __FILE__, __LINE__, msg)
#define METALL_ERRNO_TRACE(msg) metall_errno_log(METALL_LL_TRACE, __FILE__, __LINE__, msg)

#endif // METALL_LOGGER_H
