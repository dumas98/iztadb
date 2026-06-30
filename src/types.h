//
// Created by Daniel Dumas on 05/06/26.
//

#pragma once
#include <string>

/**
 * @brief Represents the type of stored value.
 * VALUE indicates a live record
 * TOMBSTONE indicates a deletion marker
 * CLOSE marks EOF
 */
enum class ValueType {
    VALUE,
    TOMBSTONE,
    CLOSE
};

/**
 * @brief A single record.
 * Bundles value type (TOMBSTONE or live VALUE) and associated value together.
 */
struct Entry {
    ValueType type;
    std::string value;
};

/**
 * @brief Represents a single record in the Write-Ahead Log.
 *
 * Fixed size layout of 393 bytes per record. Padding is disabled
 * via pragma pack to ensure exact byte layout on disk.
 * Sequence_number is an increasing sequence to identify record,
 * CRC32 checksum is used to detect corruption on recovery, type
 * indicates it's value of ValueType.
 *
 * Keys will only support strings with a maximum of 128 chars
 * and values with a maximum of 256 chars.
 */
#pragma pack(1)
struct WALRecord {
    uint32_t sequence_number;  // 4 bytes
    uint32_t checksum;         // 4 bytes
    uint8_t type;              // 1 byte
    char key[128];             // 128 bytes
    char value[256];           // 256 bytes
};
#pragma pack()