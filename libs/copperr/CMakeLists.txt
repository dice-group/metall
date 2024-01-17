cmake_minimum_required(VERSION 3.22)
set(lib "${PROJECT_NAME}-${PROJECT_NAME}")

find_package(Boost REQUIRED COMPONENTS)
find_package(Threads REQUIRED)

add_library(${lib} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${lib})

target_include_directories(${lib}
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_link_libraries(${lib}
        INTERFACE
        Threads::Threads
        Boost::headers
        )

set_target_properties(${lib}
        PROPERTIES
        CXX_STANDARD 20
        )

include(${CMAKE_SOURCE_DIR}/cmake/install_components.cmake)
install_component(INTERFACE ${PROJECT_NAME} src)
