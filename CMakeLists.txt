cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages inside system (required to continue).
set(ZLIB_ROOT /opt/homebrew/opt/zlib)
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
        src/log_utils.cpp
        src/log_reader.cpp
        src/sstable_writer.cpp
        src/sstable_reader.cpp
        src/sstable_utils.cpp
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
        src/log_utils.cpp
)
target_include_directories(LogWriterTest PRIVATE src)
target_link_libraries(LogWriterTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(LogWriterTest)

# LogReader.
add_executable(LogReaderTest)
target_sources(LogReaderTest
        PRIVATE
        tests/log_reader_test.cpp
        src/log_reader.cpp
        src/log_writer.cpp
        src/log_utils.cpp
        src/memtable.cpp
)
target_include_directories(LogReaderTest PRIVATE src)
target_link_libraries(LogReaderTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(LogReaderTest)

# SSTableWriter.
add_executable(SSTableWriterTest)
target_sources(SSTableWriterTest
        PRIVATE
        tests/sstable_writer_test.cpp
        src/sstable_writer.cpp
        src/sstable_utils.cpp
        src/memtable.cpp
)
target_include_directories(SSTableWriterTest PRIVATE src)
target_link_libraries(SSTableWriterTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(SSTableWriterTest)

# SSTableReader.
add_executable(SSTableReaderTest)
target_sources(SSTableReaderTest
        PRIVATE
        tests/sstable_reader_test.cpp
        src/sstable_writer.cpp
        src/sstable_utils.cpp
        src/memtable.cpp
        src/sstable_reader.cpp
)
target_include_directories(SSTableReaderTest PRIVATE src)
target_link_libraries(SSTableReaderTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(SSTableReaderTest)

# IztaDB.
add_executable(IztaDBTest)
target_sources(IztaDBTest
        PRIVATE
        tests/iztadb_test.cpp
        src/iztadb.cpp
        src/log_reader.cpp
        src/log_writer.cpp
        src/log_utils.cpp
        src/sstable_writer.cpp
        src/sstable_reader.cpp
        src/sstable_utils.cpp
        src/memtable.cpp
)
target_include_directories(IztaDBTest PRIVATE src)
target_link_libraries(IztaDBTest GTest::gtest_main ZLIB::ZLIB)
gtest_discover_tests(IztaDBTest)