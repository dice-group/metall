add_copperr_executable(simple simple.cpp)

add_copperr_executable(vector_of_vectors vector_of_vectors.cpp)

add_copperr_executable(string string.cpp)

add_copperr_executable(string_map string_map.cpp)

add_copperr_executable(complex_map complex_map.cpp)

add_copperr_executable(offset_pointer offset_pointer.cpp)

add_copperr_executable(snapshot snapshot.cpp)

add_copperr_executable(csr_graph csr_graph.cpp)

add_copperr_executable(fallback_allocator_adaptor fallback_allocator_adaptor.cpp)

add_copperr_executable(datastore_description datastore_description.cpp)

add_copperr_executable(object_attribute object_attribute.cpp)

add_copperr_executable(object_attribute_api_list object_attribute_api_list.cpp)

add_copperr_executable(multilevel_containers multilevel_containers.cpp)

add_copperr_executable(adjacency_list_graph adjacency_list_graph.cpp)

add_copperr_executable(static_mutex static_mutex.cpp)

add_copperr_executable(concurrent_map concurrent_map.cpp)

add_copperr_executable(copperr_containers copperr_containers.cpp)

add_copperr_executable(allocator_aware_type allocator_aware_type.cpp)

add_executable(logger logger.cpp)
target_link_libraries(logger ${PROJECT_NAME}::${PROJECT_NAME})
set_target_properties(logger PROPERTIES CXX_STANDARD 20)

if (BUILD_FFI)
    add_copperr_executable(c_api c_api.c)
    target_link_libraries(c_api PRIVATE ${PROJECT_NAME}::ffi)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/setup_mpi.cmake)
if (MPI_CXX_FOUND)
    add_copperr_executable(mpi_create mpi_create.cpp)
    setup_mpi_target(mpi_create)

    add_copperr_executable(mpi_open mpi_open.cpp)
    setup_mpi_target(mpi_open)
else()
    message(STATUS "Will skip building the MPI examples")
endif()

add_subdirectory(json)
add_subdirectory(container)
