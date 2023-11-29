add_metall_test_executable(ffi_test ffi_test.cpp)
target_link_libraries(ffi_test PRIVATE metall::ffi)