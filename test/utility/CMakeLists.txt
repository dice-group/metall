add_metall_test_executable(bitset_test bitset_test.cpp)

add_metall_test_executable(file_copy_test file_copy_test.cpp)
add_metall_test_executable(file_durability_test file_durability_test.cpp)
add_metall_test_executable(mmap_test mmap_test.cpp)

# This test defines fsync(2) in its executable to inject errors.
# A separate executable keeps the real fsync(2) for the other tests.
add_metall_test_executable(file_copy_fsync_error_test file_copy_fsync_error_test.cpp)
target_link_libraries(file_copy_fsync_error_test PRIVATE ${CMAKE_DL_LIBS})
