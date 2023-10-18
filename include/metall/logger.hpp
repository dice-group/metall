#ifndef METALL_LOGGER_HPP
#define METALL_LOGGER_HPP

#include <cstring>
#include <format>
#include <type_traits>

#include <metall/logger_interface.h>

namespace metall {
  namespace logger {
    enum struct level : std::underlying_type_t<metall_log_level> {
      error = METALL_LL_ERROR,
      warn  = METALL_LL_WARN,
      info  = METALL_LL_INFO,
      debug = METALL_LL_DEBUG,
      trace = METALL_LL_TRACE,
    };

    template<typename ...Args>
    void log(level lvl, char const *function, std::format_string<Args...> fmt, Args &&...args) {
      auto const msg = std::format(fmt, std::forward<Args>(args)...);
      metall_log(static_cast<metall_log_level>(lvl), function, msg.c_str());
    }

    template<typename ...Args>
    void errno_log(level lvl, char const *function, std::format_string<Args...> fmt, Args &&...args) {
      auto msg = std::format(fmt, std::forward<Args>(args)...);
      msg += std::format(": {}", strerror(errno));
      metall_log(static_cast<metall_log_level>(lvl), function, msg.c_str());
    }

  } // namespace logger
} // namespace logger

#define METALL_LOG(lvl, fmt, ...) ::metall::logger::log((lvl), __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)

#define METALL_ERROR(fmt, ...) METALL_LOG(::metall::logger::level::error, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_WARN(fmt, ...) METALL_LOG(::metall::logger::level::warn, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_INFO(fmt, ...) METALL_LOG(::metall::logger::level::info, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_DEBUG(fmt, ...) METALL_LOG(::metall::logger::level::debug, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_TRACE(fmt, ...) METALL_LOG(::metall::logger::level::trace, (fmt) __VA_OPT__(,) __VA_ARGS__)

#define METALL_ERRNO_LOG(lvl, fmt, ...) ::metall::logger::errno_log((lvl), __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)

#define METALL_ERRNO_ERROR(fmt, ...) METALL_ERRNO_LOG(::metall::logger::level::error, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_ERRNO_WARN(fmt, ...) METALL_ERRNO_LOG(::metall::logger::level::warn, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_ERRNO_INFO(fmt, ...) METALL_ERRNO_LOG(::metall::logger::level::info, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_ERRNO_DEBUG(fmt, ...) METALL_ERRNO_LOG(::metall::logger::level::debug, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_ERRNO_TRACE(fmt, ...) METALL_ERRNO_LOG(::metall::logger::level::trace, (fmt) __VA_OPT__(,) __VA_ARGS__)

#endif  // METALL_LOGGER_HPP
