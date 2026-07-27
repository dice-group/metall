# The benchmark holds both segment backends in one binary, so it must not get
# METALL_USE_PRIVATEER: that macro turns metall::manager into the privateer
# manager, and here the default manager is the other arm. That is why this
# target does not go through add_metall_executable.
add_executable(run_segment_backend_bench run_segment_backend_bench.cpp)
target_include_directories(run_segment_backend_bench PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(run_segment_backend_bench PRIVATE Threads::Threads)
add_common_compile_options(run_segment_backend_bench)

if (BOOST_INCLUDE_ROOT)
    target_include_directories(run_segment_backend_bench PRIVATE ${BOOST_INCLUDE_ROOT})
else ()
    target_link_libraries(run_segment_backend_bench PRIVATE ${BOOST_LIBS})
endif ()

foreach (X ${COMPILER_DEFS})
    target_compile_definitions(run_segment_backend_bench PRIVATE ${X})
endforeach ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
        NOT (CMAKE_CXX_COMPILER_ID MATCHES "Darwin" OR CMAKE_CXX_COMPILER_ID MATCHES "FreeBSD"))
    target_compile_options(run_segment_backend_bench PRIVATE -stdlib=libc++)
endif ()

if (USE_PRIVATEER)
    target_link_libraries(run_segment_backend_bench PRIVATE privateer::privateer)
    target_compile_definitions(run_segment_backend_bench PRIVATE METALL_BENCH_WITH_PRIVATEER)
endif ()
