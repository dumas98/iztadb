//
// Created by Daniel Dumas on 02/06/26.
//

#include <map>
#include <string>
#include <optional>
#include "iztadb.h"

using namespace std;

/**
 * @brief In-memory write buffer for IztaDB.
 * Stores key-value pairs in a sorted map before flushing to disk as an SSTable.
 * Reads should check the MemTable before consulting SSTables, because it holds
 * most recent writes.
 *
 */
class MemTable {
    private:
    map<string, Entry> table;

    public:
    MemTable() = default;

    /**
     * @brief Inserts a key value pair into the table.
     * Overwrites if key already exists.
     *
     * @param key   the key to insert
     * @param value the value to insert
     */
    void put(const string& key, const string& value) {
        table[key] = { ValueType::VALUE, value};
    }

    /**
     * @brief Retrieves the value associated with a given key.
     * Returns nothing if the key does not exist or has been deleted.
     *
     * @param key the key to search for
     * @return the value if found, nullopt if key does not exist or is a tombstone
     */
    optional<string> get(const string& key) {
        // Search for key first to avoid writing an empty record.
        auto it = table.find(key);

        // Only return if record wasn't deleted and it exists in table.
        if ((it != table.end()) && (it->second.type != ValueType::TOMBSTONE)) {
            // Second is the value.
            return it->second.value;
        }
        return nullopt;
    }

    /**
     * @brief Marks a key as deleted by writing a tombstone record.
     * Does not erase the key.
     *
     * @param key the key to delete
     */
    void remove(const string& key) {
        // Search for key first to avoid writing an empty record.
        auto it = table.find(key);
        if ((it != table.end()) && (it->second.type == ValueType::VALUE)) {
            table[key] = { ValueType::TOMBSTONE, ""};
        }
    }
};