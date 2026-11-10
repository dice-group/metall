add_metall_test_executable(bitset_test bitset_test.cpp)

add_metall_test_executable(file_copy_test file_copy_test.cpp)
add_metall_test_executable(file_durability_test file_durability_test.cpp)
add_metall_test_executable(mmap_test mmap_test.cpp)

# This test defines copy_file_range(2) in its executable to control the results.
# A separate executable keeps the real copy_file_range(2) for the other tests.
# copy_file_range(2) exists only on Linux.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_metall_test_executable(file_copy_range_test file_copy_range_test.cpp)
    target_link_libraries(file_copy_range_test PRIVATE ${CMAKE_DL_LIBS})
endif()
