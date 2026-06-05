//
// Created by Daniel Dumas on 04/06/26.
//

#pragma once

#include <map>
#include "types.h"


/**
 * @brief In-memory write buffer for IztaDB.
 * Stores key-value pairs in a sorted map before flushing to disk as an SSTable.
 * Reads should check the MemTable before consulting SSTables, because it holds
 * most recent writes.
 *
 */
class MemTable {
    private:
        std::map<std::string, Entry> table;

    public:
        MemTable() = default;

        /**
         * @brief Inserts a key value pair into the table.
         * Overwrites if key already exists.
         *
         * @param key the key to insert
         * @param value the value to insert
         */
        void put(const std::string& key, const std::string& value);

        /**
         * @brief Retrieves the value associated with a given key.
         * Returns nothing if the key does not exist or has been deleted.
         *
         * @param key the key to search for
         * @return the value if found, nullopt if key does not exist or is a tombstone
         */
        std::optional<std::string> get(const std::string& key);

        /**
         * @brief Marks a key as deleted by writing a tombstone record.
         * Does not erase the key.
         *
         * @param key the key to delete
         */
        void remove(const std::string& key);
};

