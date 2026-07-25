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
        iztadb1 = new IztaDB(wal_path1, sst_path1, 10,  32 * 1024 * 1024, 4096);
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
    EXPECT_EQ(iztadb1->get_block_size(), 4096);
}

// Default constructor.
TEST_F(IztaDBTest, ConstructorAssignsAttributesCorrectlyDefault) {

    IztaDB iztadb_default;
    EXPECT_EQ(iztadb_default.get_wal_path(), "./data/wal");
    EXPECT_EQ(iztadb_default.get_sst_path(), "./data/sst");
    EXPECT_EQ(iztadb_default.get_memtable_threshold(), 4 * 1024 * 1024);
    EXPECT_EQ(iztadb_default.get_max_wal_file_size(), 32 * 1024 * 1024);
    EXPECT_EQ(iztadb_default.get_block_size(), 4096);
}

// 2. Standard Operations
TEST_F(IztaDBTest, HundredPutsRoundTrip) {
    int total_records = 98;

    // Insert values using notation key000:value000, key001:value001, ...
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        iztadb1->put(key, value);
    }

    // Check only 9 files were flushed.
    ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000009.sst"));
    ASSERT_FALSE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000010.sst"));

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
    int total_records = 98;

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

    // All must be gone.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        EXPECT_EQ(iztadb1->get(key), std::nullopt);
    }
}

// Shadowing, write values and check they were flushed.
// Remove a value and it must live in MemTable
TEST_F(IztaDBTest, TombstoneInMemTableShadowsOlderFlushedValue) {
    // Fill to the threshold so one flush is executed.
    for (int i = 0; i < 10; ++i) {
        iztadb1->put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    // Check data was flushed and record exists in SSTable.
    ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000001.sst"));
    ASSERT_EQ(iztadb1->get("key1"), "value1");

    // Delete key1, it now lives in MemTable.
    iztadb1->remove("key1");

    EXPECT_EQ(iztadb1->get("key1"), std::nullopt);

    // Other keys still live in SSTables
    EXPECT_EQ(iztadb1->get("key3"), "value3");
    EXPECT_EQ(iztadb1->get("key4"), "value4");
}

// Check flush boundary.
TEST_F(IztaDBTest, FlushTriggersExactlyAtThreshold) {
    int boundary = 10;

    // Write values up to boundary, no SSTable must exist.
    for (int i = 0; i < boundary - 1; ++i) {
        iztadb1->put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000001.sst"));
    // Now add value and SSTable must exist.
    iztadb1->put("key" + std::to_string(boundary - 1), "value" + std::to_string(boundary - 1));
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000001.sst"));
}

// 3. Crash restart
TEST_F(IztaDBTest, CrashRestartRecoversFromWAL) {
    // Write data and guarantee there's no flushing.
    iztadb1->put("key1", "value1");
    iztadb1->put("key2", "value2");
    iztadb1->put("key3", "value3");
    iztadb1->remove("key2");

    // Confirm data was logged before the "crash".
    ASSERT_EQ(iztadb1->get("key1"), "value1");
    ASSERT_EQ(iztadb1->get("key2"), std::nullopt);
    ASSERT_EQ(iztadb1->get("key3"), "value3");

    // Simulate crash.
    delete iztadb1;

    // Reboot IztaDB
    iztadb1 = new IztaDB(wal_path1, sst_path1, 10, 32 * 1024 * 1024, 4096);

    // All three keys should be queryable, with no explicit
    EXPECT_EQ(iztadb1->get("key1"), "value1");
    EXPECT_EQ(iztadb1->get("key2"), std::nullopt);
    EXPECT_EQ(iztadb1->get("key3"), "value3");
}

// Reboot with SSTables present but no WAL.
TEST_F(IztaDBTest, StartWithSSTablesButNoWAL) {

    // Fill to the threshold so one flush is executed.
    for (int i = 0; i < 10; ++i) {
        iztadb1->put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(sst_path1) / "000001.sst"));

    // Test the flush cleared WAL files.
    bool wal_has_log_files = false;
    for (const auto& entry : std::filesystem::directory_iterator(wal_path1)) {
        if (entry.path().extension() == ".log") wal_has_log_files = true;
    }
    ASSERT_FALSE(wal_has_log_files);

    // Reboot IztaDB.
    delete iztadb1;
    iztadb1 = new IztaDB(wal_path1, sst_path1, 10, 32 * 1024 * 1024, 4096);

    // Values should be reachable via SSTables.
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(iztadb1->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }
}




