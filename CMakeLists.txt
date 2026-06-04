cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        DOWNLOAD_EXTRACT_TIMESTAMP true
)

project(IztaDB)

add_executable(Stage1)

target_sources(Stage1
        PRIVATE
        src/main.cpp
        src/unordered_map_commands.cpp
        src/iztadb.h
        src/memtable.cpp
)
