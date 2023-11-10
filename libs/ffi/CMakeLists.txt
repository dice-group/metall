cmake_minimum_required(VERSION 3.21)
set(lib_suffix "ffi")
set(lib "${PROJECT_NAME}-${lib_suffix}")

add_library(${lib}
        src/dice/ffi/metall.cpp
        )
add_library(${PROJECT_NAME}::${lib_suffix} ALIAS ${lib})

target_link_libraries(${lib}
        PUBLIC
        ${PROJECT_NAME}::${PROJECT_NAME}
        )

target_include_directories(${lib}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        )

set_target_properties(${lib}
        PROPERTIES
        CXX_STANDARD 20
        )

## Packaging and exporting the target
include(${CMAKE_SOURCE_DIR}/cmake/install_components.cmake)
install_component(PUBLIC ${lib_suffix} src)
