cmake_minimum_required(VERSION 3.21)

add_metall_executable(datastore_ls
        src/dice/metall/datastore_ls.cpp
        )

add_metall_executable(mpi_datastore_ls
        src/dice/metall/mpi_datastore_ls.cpp
        )