cmake_minimum_required(VERSION 3.21)
set(lib_suffix "default-logger")
set(lib "${PROJECT_NAME}-${lib_suffix}")

add_library(${lib}
        src/dice/metall/default_logger.cpp
        )
add_library(${PROJECT_NAME}::${lib_suffix} ALIAS ${lib})

target_include_directories(${lib}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        )

target_link_libraries(${lib}
        PRIVATE
        ${PROJECT_NAME}::${PROJECT_NAME}
        )

include(${CMAKE_SOURCE_DIR}/cmake/install_components.cmake)
install_component(PUBLIC ${lib_suffix} src)
