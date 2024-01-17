add_copperr_executable(open_metall open_metall.cpp)
add_copperr_executable(extend_metall extend_metall.cpp)
add_copperr_executable(destroy_metall destroy_metall.cpp)
add_copperr_executable(compare_key_value_lists compare_key_value_lists.cpp)

configure_file(test.sh test.sh COPYONLY)
configure_file(test_large.sh test_large.sh COPYONLY)

add_subdirectory(data)