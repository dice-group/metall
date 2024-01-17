cmake_minimum_required(VERSION 3.22)
add_subdirectory(copperr)

if (BUILD_FFI)
    add_subdirectory(ffi)
endif ()

if (WITH_DEFAULT_LOGGER)
    add_subdirectory(default-logger)
endif ()

include(${CMAKE_SOURCE_DIR}/cmake/install_components.cmake)
install_package()
