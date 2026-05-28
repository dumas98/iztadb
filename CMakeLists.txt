cmake_minimum_required(VERSION 3.23)

project(IztaDB)

add_executable(HelloWorld)

target_sources(HelloWorld
        PRIVATE
        src/main.cpp
)
