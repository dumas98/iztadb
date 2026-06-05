//
// Created by Daniel Dumas on 03/06/26.
//

#include <gtest/gtest.h>
#include "memtable.h"

/**
 * @brief Test fixture for MemTable.
 * Builds a MemTable instance before each test.
 *
 */
class MemTableTest : public testing::Test {
    protected:
    MemTable mem_table;

    // Before Each.
    void SetUp() override {
        mem_table = MemTable();
    }
};

// Verifies Put works correctly when trying to retrieve a previously inserted value.
TEST_F(MemTableTest, PutAndGet) {
    mem_table.put("name1", "Daniel");
    mem_table.put("lastname1", "Dumas");
    EXPECT_EQ(mem_table.get("name1"), "Daniel");
    EXPECT_EQ(mem_table.get("lastname1"), "Dumas");
}

// Verifies that putting an existing key overwrites the value.
TEST_F(MemTableTest, PutOverwritesExistingKey) {
    mem_table.put("DB Name", "IztaDB");
    mem_table.put("DB Name", "LevelDB");
    EXPECT_EQ(mem_table.get("DB Name"), "LevelDB");
}

// Verifies that get returns nullopt for a key that does not exist.
TEST_F(MemTableTest, GetNonExistentKeyReturnsNullopt) {
    EXPECT_EQ(mem_table.get("DB"), nullopt);
}



