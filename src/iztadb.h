//
// Created by Daniel Dumas on 29/05/26.
//

#pragma once
#include <string>

using namespace std;

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
    string value;
};
