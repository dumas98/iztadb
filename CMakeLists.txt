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
        src/iztadb.cpp
        src/memtable.cpp
        src/log_writer.cpp
)

target_include_directories(MainExecutable PRIVATE src)

# Add libraries.
target_link_libraries(MainExecutable PRIVATE ZLIB::ZLIB)

# Setting up Tests.
enable_testing()
include(GoogleTest)

# MemTable.
add_executable(MemTableTest)
target_sources(MemTableTest
        PRIVATE
        tests/memtable_test.cpp
        src/memtable.cpp
)

target_include_directories(MemTableTest PRIVATE src)
target_link_libraries(MemTableTest GTest::gtest_main)

gtest_discover_tests(MemTableTest)

# LogWriter.
add_executable(LogWriterTest)
target_sources(LogWriterTest
        PRIVATE
        tests/log_writer_test.cpp
        src/log_writer.cpp
)
target_include_directories(LogWriterTest PRIVATE src)
target_link_libraries(LogWriterTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(LogWriterTest)