#ifndef METALL_LOGGER_INTERFACE_H
#define METALL_LOGGER_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Level of log messages
 */
typedef enum metall_log_level {
  /// Errors
  METALL_LL_ERROR = 0,
  /// Warnings
  METALL_LL_WARN = 1,
  /// Informational
  METALL_LL_INFO = 2,
  /// Verbose logging
  METALL_LL_DEBUG = 3,
  /// Very Verbose logging
  METALL_LL_TRACE = 4,
} metall_log_level;

/// \brief Log a message
void metall_log(metall_log_level lvl, char const *function, char const *message);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // METALL_LOGGER_INTERFACE_H
