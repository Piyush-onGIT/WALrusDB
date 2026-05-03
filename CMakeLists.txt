cmake_minimum_required(VERSION 3.14)
project(walrusdb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WALRUSDB_BUILD_TESTS "Build tests" ON)

# library
add_library(walrusdb
    db/db.cc
    db/memtable.cc
    db/skip_list.cc
    db/wal.cc
    db/writer_batch.cc
    db/writer.cc
)

target_include_directories(walrusdb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/db
)

# tests
if(WALRUSDB_BUILD_TESTS)
    add_executable(skip_list_test db/skip_list_test.cc)
    target_link_libraries(skip_list_test PRIVATE walrusdb)
endif()
