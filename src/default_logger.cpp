#include <metall/logger_interface.h>

#include <iostream>

static char const *log_level_to_string(metall_log_level lvl) noexcept {
  switch (lvl) {
    case METALL_LL_ERROR: {
      return "ERROR";
    }
    case METALL_LL_WARN: {
      return "WARNING";
    }
    case METALL_LL_INFO: {
      return "INFO";
    }
    case METALL_LL_DEBUG: {
      return "DEBUG";
    }
    case METALL_LL_TRACE: {
      return "TRACE";
    }
  }
}

extern "C" void metall_log(metall_log_level lvl,
                           [[maybe_unused]] char const *file,
                           [[maybe_unused]] size_t line,
                           char const *function,
                           char const *message) {
  std::cerr << log_level_to_string(lvl) << " (" << function << "): " << message << std::endl;
}
