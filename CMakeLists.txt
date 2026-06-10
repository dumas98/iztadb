cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages inside system (required to continue).
set(ZLIB_ROOT /usr/local/Cellar/zlib/1.3.2)
find_package(ZLIB REQUIRED)


# Get Google Test.
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(googletest)

# Define Project.
project(IztaDB)

add_executable(MainExecutable)

target_sources(MainExecutable
        PRIVATE
        src/main.cpp
        src/types.h
        src/iztadb.cpp
        src/iztadb.h
        src/memtable.cpp
        src/memtable.h
        src/log_writer.cpp
        src/log_writer.h
)

# Add libraries.
target_link_libraries(MainExecutable PRIVATE ZLIB::ZLIB)

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
