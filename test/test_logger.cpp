#include <metall/logger.h>

#include <iostream>

static char const *log_level_to_string(metall_log_level lvl) noexcept {
  switch (lvl) {
    case METALL_LL_ERROR: {
      return "ERROR";
    }
    case METALL_LL_WARN: {
      return "WARNING";
    }
    case METAL_LL_INFO: {
      return "INFO";
    }
    case METAL_LL_DEBUG: {
      return "DEBUG";
    }
    case METALL_LL_TRACE: {
      return "TRACE";
    }
  }
}

extern "C" void metall_log([[maybe_unused]] metall_log_level lvl, char const *file_name, uint64_t line, char const *message) {
  std::cerr << log_level_to_string(lvl) << ' ' << file_name << ':' << line << ": " << message << std::endl;
}

extern "C" void metall_errno_log([[maybe_unused]] metall_log_level lvl, char const *file_name, uint64_t line, char const *message) {
  std::cerr << log_level_to_string(lvl) << ' ' << file_name << ':' << line << ": ";
  std::perror(message);
}
