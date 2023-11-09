cmake_minimum_required(VERSION 3.22)
project(metall VERSION 0.26.0
        DESCRIPTION "TODO")

include(cmake/boilerplate_init.cmake)
boilerplate_init()

option(WITH_DEFAULT_LOGGER "build and use metall's default logger" ON)
option(BUILD_FFI "build foreign function interface" OFF)
option(BUILD_TESTING "build tests" OFF)
option(BUILD_UTILITY "build utility executables" OFF)
option(BUILD_EXAMPLES "build example executables" OFF)
option(BUILD_BENCHMARKS "build benchmarks" OFF)
option(BUILD_VERIFICATION "build verification executables" OFF)
option(RUN_LARGE_SCALE_TESTS "build and run large scale tests" OFF)

set(DEFAULT_VM_RESERVE_SIZE "0" CACHE STRING
        "Set the default VM reserve size (use the internally defined value if 0 is specified)")
set(MAX_SEGMENT_SIZE "0" CACHE STRING
        "Set the max segment size (use the internally defined value if 0 is specified)")
set(INITIAL_SEGMENT_SIZE "0" CACHE STRING
        "Set the initial segment size (use the internally defined value if 0 is specified)")

# ---------- Experimental options ---------- #
option(USE_ANONYMOUS_NEW_MAP "Use the anonymous map when creating a new map region" OFF)
set(UMAP_ROOT "" CACHE PATH "UMap installed root directory")
set(FREE_SMALL_OBJECT_SIZE_HINT "0" CACHE STRING
        "Try to free the associated pages and file space when objects equal to or larger than that is deallocated")



OPTION(USE_CONAN "If available, use conan to retrieve dependencies." ON)
if (PROJECT_IS_TOP_LEVEL AND USE_CONAN)
    include(cmake/conan_cmake.cmake)
    if (BUILD_TESTING)
        set(CONAN_METALL_WITH_TEST_DEPS "True")
    else()
        set(CONAN_METALL_WITH_TEST_DEPS "False")
    endif()

    if (BUILD_FFI)
        set(CONAN_METALL_BUILD_FFI "True")
    else ()
        set(CONAN_METALL_BUILD_FFI "False")
    endif ()

    if (WITH_DEFAULT_LOGGER)
        set(CONAN_METALL_WITH_DEFAULT_LOGGER "True")
    else ()
        set(CONAN_METALL_WITH_DEFAULT_LOGGER "False")
    endif ()

    set(CONAN_OPTIONS "with_test_deps=${CONAN_METALL_WITH_TEST_DEPS};build_ffi=${CONAN_METALL_BUILD_FFI};with_default_logger=${CONAN_METALL_WITH_DEFAULT_LOGGER}")
    install_packages_via_conan("${CMAKE_SOURCE_DIR}/conanfile.py" "${CONAN_OPTIONS}")
endif ()

find_package(Boost REQUIRED COMPONENTS)
find_package(Threads REQUIRED)

function(add_common_compile_options name)
    # Common
    set_target_properties(${name} PROPERTIES CXX_STANDARD 20)
    target_compile_options(${name} PRIVATE -Wall)

    # Debug
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Og>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-g3>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Wextra>)
    if (Linux)
        target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-pg>)
    endif ()

    # Release
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-DNDEBUG>)

    # Release with debug info
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-g3>)
    if (Linux)
        target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-pg>)
    endif ()
endfunction()

function(common_setup_for_metall_executable name)
    target_link_libraries(${name}
            PRIVATE
            ${PROJECT_NAME}::${PROJECT_NAME}
            -Wl,--whole-archive ${PROJECT_NAME}::default-logger -Wl,--no-whole-archive
            )
    add_common_compile_options(${name})

    if (USE_ANONYMOUS_NEW_MAP)
        if (UMAP_ROOT)
            message(FATAL_ERROR "USE_ANONYMOUS_NEW_MAP and UMAP_ROOT options cannot coexist")
        endif ()

        target_compile_definitions(${name} PRIVATE METALL_USE_ANONYMOUS_NEW_MAP)
    endif ()

    if (UMAP_ROOT)
        target_include_directories(${name} PRIVATE ${UMAP_ROOT}/include)
        if (LIBUMAP)
            target_link_libraries(${name} PRIVATE ${LIBUMAP})
            target_compile_definitions(${name} PRIVATE "METALL_USE_UMAP")
        endif ()
    endif ()
endfunction()

function(add_metall_executable name source)
    add_executable(${name} ${source})
    common_setup_for_metall_executable(${name})
endfunction ()


add_subdirectory(libs)

if (PROJECT_IS_TOP_LEVEL)
    if (BUILD_DOC)
        include(cmake/build_doc.cmake)
        build_doc()
    endif ()

    if (BUILD_TESTING)
        include(CTest)
        enable_testing()
        add_subdirectory(tests)
    endif ()

    if (BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif ()

    if (BUILD_VERIFICATION)
        add_subdirectory(verification)
    endif ()

    if (BUILD_BENCHMARKS)
        add_subdirectory(bench)
    endif ()

    if (BUILD_UTILITY)
        add_subdirectory(execs)
    endif ()
endif ()
