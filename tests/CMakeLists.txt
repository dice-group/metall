find_package(GTest REQUIRED)
include(GoogleTest)

function(add_metall_test_executable name source)
    add_metall_executable(${name} ${source})

    target_link_libraries(${name} PRIVATE GTest::gtest_main GTest::gtest)
    gtest_discover_tests(${name})
    if (RUN_LARGE_SCALE_TESTS)
        target_compile_definitions(${name} PRIVATE "METALL_RUN_LARGE_SCALE_TEST")
    endif()
endfunction()

add_subdirectory(utility)
add_subdirectory(kernel)
add_subdirectory(container)
