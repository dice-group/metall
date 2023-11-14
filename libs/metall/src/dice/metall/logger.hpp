#ifndef METALL_LOGGER_HPP
#define METALL_LOGGER_HPP

#include <cstring>
#include <format>
#include <source_location>
#include <type_traits>

#include <dice/metall/logger_interface.h>

namespace dice::metall::logger {
  /**
   * Level of log messages
   */
  enum struct level : std::underlying_type_t<metall_log_level> {
    error = METALL_LL_ERROR,
    warn  = METALL_LL_WARN,
    info  = METALL_LL_INFO,
    debug = METALL_LL_DEBUG,
    trace = METALL_LL_TRACE,
  };

  /**
   * Logs a message
   *
   * @param lvl log level
   * @param function function the message originated from
   * @param fmt format specifier for the message
   * @param args arguments for fmt
   */
  template<typename ...Args>
  void log(level lvl, std::source_location sloc, std::format_string<Args...> fmt, Args &&...args) {
    auto const msg = std::format(fmt, std::forward<Args>(args)...);
    metall_log(static_cast<metall_log_level>(lvl), sloc.file_name(), sloc.line(), sloc.function_name(), msg.c_str());
  }

  /**
   * Logs a message about the current value of errno, similar to perror
   *
   * @param lvl log level
   * @param function function the message origininated from
   * @param fmt format specifier for the message
   * @param args arguments for fmt
   */
  template<typename ...Args>
  void errno_log(level lvl, std::source_location sloc, std::format_string<Args...> fmt, Args &&...args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
    msg += std::format(": {}", strerror(errno));
    metall_log(static_cast<metall_log_level>(lvl), sloc.file_name(), sloc.line(), sloc.function_name(), msg.c_str());
  }
} // namespace dice::metall::logger

/**
 * Convenience macro for calling dice::metall::logger::log.
 * Automatically populates the function argument with the name of the current function, and forwards the rest (i.e. lvl, fmt, args...)
 */
#define METALL_LOG(lvl, fmt, ...) ::dice::metall::logger::log((lvl), ::std::source_location::current(), (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_LOG that sets lvl to error
 */
#define METALL_ERROR(fmt, ...) METALL_LOG(::dice::metall::logger::level::error, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_LOG that sets lvl to warn
 */
#define METALL_WARN(fmt, ...) METALL_LOG(::dice::metall::logger::level::warn, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_LOG that sets lvl to info
 */
#define METALL_INFO(fmt, ...) METALL_LOG(::dice::metall::logger::level::info, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_LOG that sets lvl to debug
 */
#define METALL_DEBUG(fmt, ...) METALL_LOG(::dice::metall::logger::level::debug, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_LOG that sets lvl to trace
 */
#define METALL_TRACE(fmt, ...) METALL_LOG(::dice::metall::logger::level::trace, (fmt) __VA_OPT__(,) __VA_ARGS__)


/**
 * Convenience macro for calling dice::metall::logger::errno_log.
 * Automatically populates the function argument with the name of the current function, and forwards the rest (i.e. lvl, fmt, args...)
 */
#define METALL_ERRNO_LOG(lvl, fmt, ...) ::dice::metall::logger::errno_log((lvl), ::std::source_location::current(), (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_ERRNO_LOG that sets lvl to error
 */
#define METALL_ERRNO_ERROR(fmt, ...) METALL_ERRNO_LOG(::dice::metall::logger::level::error, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_ERRNO_LOG that sets lvl to warn
 */
#define METALL_ERRNO_WARN(fmt, ...) METALL_ERRNO_LOG(::dice::metall::logger::level::warn, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_ERRNO_LOG that sets lvl to info
 */
#define METALL_ERRNO_INFO(fmt, ...) METALL_ERRNO_LOG(::dice::metall::logger::level::info, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_ERRNO_LOG that sets lvl to debug
 */
#define METALL_ERRNO_DEBUG(fmt, ...) METALL_ERRNO_LOG(::dice::metall::logger::level::debug, (fmt) __VA_OPT__(,) __VA_ARGS__)

/**
 * Convenience macro for METALL_ERRNO_LOG that sets lvl to trace
 */
#define METALL_ERRNO_TRACE(fmt, ...) METALL_ERRNO_LOG(::dice::metall::logger::level::trace, (fmt) __VA_OPT__(,) __VA_ARGS__)

#endif  // METALL_LOGGER_HPP
