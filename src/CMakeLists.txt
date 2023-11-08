# ---------- Logger ---------- #
add_library(metall_default_logger default_logger.cpp)
add_library(${PROJECT_NAME}::default_logger ALIAS metall_default_logger)
target_link_libraries(metall_default_logger PUBLIC ${PROJECT_NAME}::${PROJECT_NAME})
set_target_properties(metall_default_logger PROPERTIES CXX_STANDARD 20)

install(TARGETS ${PROJECT_NAME} metall_default_logger
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (BUILD_UTILITY)
    add_metall_executable(datastore_ls datastore_ls.cpp)
    install(TARGETS datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    add_metall_executable(mpi_datastore_ls mpi_datastore_ls.cpp)
    install(TARGETS mpi_datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if (BUILD_C)
    if (NOT Boost_FOUND)
        return()
    endif ()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

    add_library(metall_c metall_c.cpp)
    add_library(${PROJECT_NAME}::metall_c ALIAS metall_c)

    target_include_directories(metall_c PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    common_setup_for_metall_executable(metall_c)

    install(TARGETS metall_c
            EXPORT ${PROJECT_NAME}_Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()