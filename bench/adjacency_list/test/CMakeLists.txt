add_copperr_executable(open_copperr open_copperr.cpp)
add_copperr_executable(extend_copperr extend_copperr.cpp)
add_copperr_executable(destroy_copperr destroy_copperr.cpp)
add_copperr_executable(compare_key_value_lists compare_key_value_lists.cpp)

configure_file(test.sh test.sh COPYONLY)
configure_file(test_large.sh test_large.sh COPYONLY)

add_subdirectory(data)