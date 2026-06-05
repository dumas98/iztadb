cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(googletest)

project(IztaDB)

add_executable(Stage1)

target_sources(Stage1
        PRIVATE
        src/main.cpp
        src/unordered_map_commands.cpp
        src/iztadb.h
        src/memtable.cpp
)

# Setting up Memtable test.
enable_testing()

add_executable(MemTableTest)
target_sources(MemTableTest
        PRIVATE
        tests/memtable_test.cpp
        src/memtable.cpp
)

target_include_directories(MemTableTest PRIVATE src)
target_link_libraries(MemTableTest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(MemTableTest)
