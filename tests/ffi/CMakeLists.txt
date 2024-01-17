add_copperr_test_executable(ffi_test ffi_test.cpp)
target_link_libraries(ffi_test PRIVATE copperr::ffi)