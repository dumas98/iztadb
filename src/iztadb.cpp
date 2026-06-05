//
// Created by Daniel Dumas on 05/06/26.
//

// iztadb.cpp
#include "iztadb.h"
#include <filesystem>

IztaDB::IztaDB() {
    data_path = "./data";

    // Create two directories in system for WAL and SST files.
    std::filesystem::create_directories(data_path + "/wal");
    std::filesystem::create_directories(data_path + "/sst");
}