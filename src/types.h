//
// Created by Daniel Dumas on 05/06/26.
//

#pragma once
#include <string>

/**
 * @brief Represents the type of stored value.
 * VALUE indicates a live record
 * TOMBSTONE indicates a deletion marker
 */
enum class ValueType {
    VALUE,
    TOMBSTONE
};

/**
 * @brief A single record.
 * Bundles value type (TOMBSTONE or live VALUE) and associated value together.
 */
struct Entry {
    ValueType type;
    std::string value;
};
