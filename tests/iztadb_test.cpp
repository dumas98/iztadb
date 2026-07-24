//
// Created by Daniel Dumas on 23/07/26.
//

#include <gtest/gtest.h>
#include "iztadb.h"


using namespace std;

/**
 * @brief Test fixture for SSTableWriter.
 * Builds a SSTableWriter instance before each test, adds directories and removes them after each test.
 *
 */
class IztaDBTest : public testing::Test {
protected:
    IztaDB* iztadb1;
    std::string wal_path1 = "./test_data/iztadb1/wal";
    std::string sst_path1 = "./test_data/iztadb1/sst";

    // Before Each.
    void SetUp() override {
        iztadb1 = new IztaDB(wal_path1, sst_path1, 10,  32 * 1024 * 1024);
    }

    // After each
    void TearDown() override {
        delete iztadb1;
        std::filesystem::remove_all("./test_data/");
    }
};


// 1. Test Constructors
// Test Directory Creation.
TEST_F(IztaDBTest, ConstructsCleanlyOnEmptyDirectories) {

    // Both directories should exist, created by the constructor.
    EXPECT_TRUE(std::filesystem::exists(wal_path1));
    EXPECT_TRUE(std::filesystem::exists(sst_path1));

    // New database should be empty returning nothing for non-existent keys.
    EXPECT_EQ(iztadb1->get("key1"), std::nullopt);
    EXPECT_EQ(iztadb1->get("anything"), std::nullopt);
}

// Constructor attributes.
TEST_F(IztaDBTest, ConstructorAssignsAttributesCorrectly) {
    EXPECT_EQ(iztadb1->get_wal_path(), wal_path1);
    EXPECT_EQ(iztadb1->get_sst_path(), sst_path1);
    EXPECT_EQ(iztadb1->get_memtable_threshold(), 10);
    EXPECT_EQ(iztadb1->get_max_wal_file_size(), 32 * 1024 * 1024);
}

// Default constructor.
TEST_F(IztaDBTest, ConstructorAssignsAttributesCorrectlyDefault) {

    IztaDB iztadb_default;
    EXPECT_EQ(iztadb_default.get_wal_path(), "./data/wal");
    EXPECT_EQ(iztadb_default.get_sst_path(), "./data/sst");
    EXPECT_EQ(iztadb_default.get_memtable_threshold(), 4 * 1024 * 1024);
    EXPECT_EQ(iztadb_default.get_max_wal_file_size(), 32 * 1024 * 1024);
}

// 2. Standard Operations
TEST_F(IztaDBTest, HundredPutsRoundTrip) {
    int total_records = 100;

    // Insert values using notation key000:value000, key001:value001, ...
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        iztadb1->put(key, value);
    }

    // Read values back.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string expected_value = std::format("value{:03d}", i);

        auto result = iztadb1->get(key);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expected_value);
    }
}

// Put many values and immediately delete them.
TEST_F(IztaDBTest, MultiplePutsDeleteRoundTrip) {
    int total_records = 100;

    // Insert values using notation key000:value000, key001:value001, ...
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        iztadb1->put(key, value);
    }

    // Confirm they're all live before deleting anything.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string expected_value = std::format("value{:03d}", i);

        auto result = iztadb1->get(key);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expected_value);
    }

    // Delete every key.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        iztadb1->remove(key);
    }

    // All should now be gone.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        EXPECT_EQ(iztadb1->get(key), std::nullopt);
    }
}


