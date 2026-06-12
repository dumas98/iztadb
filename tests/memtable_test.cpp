//
// Created by Daniel Dumas on 03/06/26.
//

#include <gtest/gtest.h>
#include "memtable.h"
#include <fstream>
#include <filesystem>

using namespace std;

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
    mem_table.put("DB Name1", "IztaDB");
    mem_table.put("DB Name1", "LevelDB");
    EXPECT_EQ(mem_table.get("DB Name1"), "LevelDB");
}

// Verifies that get returns nullopt for a key that does not exist.
TEST_F(MemTableTest, GetNonExistentKeyReturnsNullopt) {
    EXPECT_EQ(mem_table.get("DB Name1"), nullopt);
}

// Verifies that searching for a removed key does not exist (key must be inserted first).
TEST_F(MemTableTest, PutAndDeleteReturnsNullopt) {
    mem_table.put("DB Name1", "RocksDB");
    mem_table.remove("DB Name1");
    EXPECT_EQ(mem_table.get("DB Name1"), nullopt);
}

// Verifies that a key that was inserted, removed and inserted again retrieves the new value.
TEST_F(MemTableTest, PutRemovePut) {
    mem_table.put("DB Name1", "DuckDB");

    // After removed should return a nullopt.
    mem_table.remove("DB Name1");
    EXPECT_EQ(mem_table.get("DB Name1"), nullopt);

    // Reinsert same key-value pair.
    mem_table.put("DB Name1", "DuckDB");
    EXPECT_EQ(mem_table.get("DB Name1"), "DuckDB");
}

// Verifies characters variations.
TEST_F(MemTableTest, PutAndGetCharacterVariations) {
    // Extra Characters, Chinese.
    mem_table.put("!#$%&/()=?¡,.;:-_<>{}[]|", "你好世界");
    EXPECT_EQ(mem_table.get("!#$%&/()=?¡,.;:-_<>{}[]|"), "你好世界");

    // Digits.
    mem_table.put("123457890", "0987654321");
    EXPECT_EQ(mem_table.get("123457890"), "0987654321");

    // Character Variations and Emojis.
    mem_table.put("ƎⱯℛξρΠΨ", "😀😂😉😍😎😭👍👌❤️🔥");
    EXPECT_EQ(mem_table.get("ƎⱯℛξρΠΨ"), "😀😂😉😍😎😭👍👌❤️🔥");

    // Escape Characters. \ and "
    mem_table.put("DB \"Name1\" \\", "Apache Cassandra");
    EXPECT_EQ(mem_table.get("DB \"Name1\" \\"), "Apache Cassandra");
}

// Verifies empty string insertions.
TEST_F(MemTableTest, EmptyStrings) {
    // Both Empty Strings.
    mem_table.put("", "");
    EXPECT_EQ(mem_table.get(""), "");

    // After removed should return a nullopt.
    mem_table.remove("");
    EXPECT_EQ(mem_table.get(""), nullopt);
}



