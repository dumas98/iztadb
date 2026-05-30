cmake_minimum_required(VERSION 3.23)

project(IztaDB)

add_executable(Stage1)

target_sources(Stage1
        PRIVATE
        src/main.cpp
        src/unordered_map_commands.cpp
        src/iztadb.h
)
