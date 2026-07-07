//
// Created by Daniel Dumas on 02/06/26.
//

#include "memtable.h"
using namespace std;

void MemTable::put(const string& key, const string& value) {
    table[key] = { ValueType::VALUE, value };
}

optional<string> MemTable::get(const string& key) {
    // Search for key first to avoid writing an empty record.
    auto it = table.find(key);

    // Only return if record wasn't deleted and it exists in table.
    if ((it != table.end()) && (it->second.type != ValueType::TOMBSTONE)) {
        // Second is the value.
        return it->second.value;
    }
    return nullopt;
}

void MemTable::remove(const string& key) {
    // Search for key first to avoid writing an empty record.
    auto it = table.find(key);

    // If key exists, and it's a non-tombstone ValueType, change it to tombstone record.
    if ((it != table.end()) && (it->second.type == ValueType::VALUE)) {
        table[key] = { ValueType::TOMBSTONE, "" };
    }
}

const std::map<std::string, Entry>& MemTable::get_map() const {
    return table;
}