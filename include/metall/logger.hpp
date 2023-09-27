#ifndef METALL_LOGGER_HPP
#define METALL_LOGGER_HPP

#include <metall/logger_interface.h>

#include <format>

namespace metall {
  enum struct log_level {
    Error = METALL_LL_ERROR,
    Warn = METALL_LL_WARN,
    Info = METALL_LL_INFO,
    Debug = METALL_LL_DEBUG,
    Trace = METALL_LL_TRACE,
  };

  template<typename ...Args>
  void log(log_level lvl, char const *function, std::format_string<Args...> fmt, Args &&... args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
    metall_log(static_cast<metall_log_level>(lvl), function, msg.c_str());
  }
} // namespace metall

#define METALL_ERROR(fmt, ...) ::metall::log(::metall::log_level::Error, __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_WARN(fmt, ...) ::metall::log(::metall::log_level::Warn, __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_INFO(fmt, ...) ::metall::log(::metall::log_level::Info, __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_DEBUG(fmt, ...) ::metall::log(::metall::log_level::Debug, __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)
#define METALL_TRACE(fmt, ...) ::metall::log(::metall::log_level::Trace, __PRETTY_FUNCTION__, (fmt) __VA_OPT__(,) __VA_ARGS__)

#endif//METALL_LOGGER_HPP
