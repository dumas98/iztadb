//
// Created by Daniel Dumas on 29/05/26.
//

#pragma once
#include <string>

/**
 * @brief Core storage engine.
 * On construction, initializes the required directory structure.
 *
 */
class IztaDB {
    private:
        std::string data_path;
    /**
     * @brief Constructs core storage engine adding the following
     * directories if they do not exist:
     *  ./data/wal/  - Storage for Write-ahead log files
     *  ./data/sst/  - Sorted string table files4
     *
     */
    public:
        IztaDB();
};
