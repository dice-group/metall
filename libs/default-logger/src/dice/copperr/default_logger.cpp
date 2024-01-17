#include <dice/copperr/logger_interface.h>

#include <iostream>

static char const *log_level_to_string(copperr_log_level lvl) noexcept {
  switch (lvl) {
    case COPPERR_LL_ERROR: {
      return "ERROR";
    }
    case COPPERR_LL_WARN: {
      return "WARNING";
    }
    case COPPERR_LL_INFO: {
      return "INFO";
    }
    case COPPERR_LL_DEBUG: {
      return "DEBUG";
    }
    case COPPERR_LL_TRACE: {
      return "TRACE";
    }
    default: {
      return "UNKNOWN";
    }
  }
}

extern "C" void copperr_log(copperr_log_level lvl,
                            [[maybe_unused]] char const *file,
                            [[maybe_unused]] size_t line,
                            char const *function,
                            char const *message) {
  std::cerr << log_level_to_string(lvl) << " (" << function << "): " << message << std::endl;
}
