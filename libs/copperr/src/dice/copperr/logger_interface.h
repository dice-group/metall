#ifndef METALL_LOGGER_INTERFACE_H
#define METALL_LOGGER_INTERFACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Level of log messages
 */
typedef enum copperr_log_level {
  /// Errors
  COPPERR_LL_ERROR = 0,
  /// Warnings
  COPPERR_LL_WARN = 1,
  /// Informational
  COPPERR_LL_INFO = 2,
  /// Verbose logging
  COPPERR_LL_DEBUG = 3,
  /// Very Verbose logging
  COPPERR_LL_TRACE = 4,
} copperr_log_level;

/// \brief Log a message
void copperr_log(copperr_log_level lvl, char const *file, size_t line, char const *function, char const *message);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // METALL_LOGGER_INTERFACE_H
