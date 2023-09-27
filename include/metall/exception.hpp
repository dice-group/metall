#ifndef METALL_EXCEPTION_HPP
#define METALL_EXCEPTION_HPP

#include <stdexcept>

namespace metall {
  struct exception : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  struct system_exception : exception, std::system_error {
    using std::system_error::system_error;
  };

  struct management_data_read_error : exception {
    using exception::exception;
  };

  struct management_data_write_error : exception {
    using exception::exception;
  };

} // namespace metall

#endif  // METALL_EXCEPTION_HPP
