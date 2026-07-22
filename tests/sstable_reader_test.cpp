//
// Created by Daniel Dumas on 21/07/26.
//

#include <gtest/gtest.h>
#include "memtable.h"
#include "sstable_utils.h"
#include "sstable_writer.h"
#include "sstable_reader.h"
#include <random>
#include <set>

using namespace std;

/**
 * @brief Test fixture for SSTableReaderTest.
 *
 */
class SSTableReaderTest : public testing::Test {
protected:
    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    // Variable structure: sstable_writer_{block size}
    SSTableWriter* sstable_writer_1kb;
    SSTableReader* sstable_reader;
    string sst_path_1kb = "./test_data/sst_1kb";

    MemTable* mem_table;

    // Before Each.
    void SetUp() override {
        std::filesystem::create_directories(sst_path_1kb);

        mem_table = new MemTable();

    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete sstable_writer_1kb;
        delete sstable_reader;

        // Drop test directory.
        std::filesystem::remove_all("./test_data/");
        std::cout << "Test Ended." << std::endl;
    }

    // Test helper, corrupt one byte inside a file.
    void corrupt_byte_at(const std::filesystem::path& path, uintmax_t offset) {
        std::fstream corrupt(path, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(corrupt.is_open());

        char original_byte;
        corrupt.seekg(offset);
        corrupt.read(&original_byte, 1);

        char flipped_byte = original_byte ^ 0xFF;
        corrupt.seekp(offset);
        corrupt.write(&flipped_byte, 1);
    }
};

// 1. Test Constructor
TEST_F(SSTableReaderTest, ConstructorInitializesCorrectly) {

    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Get indexes from SSTableWriter.
    std::vector<IndexEntry> written_entries = sstable_writer_1kb->get_index_entries();
    ASSERT_FALSE(written_entries.empty());

    // Check .sst file exists.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    sstable_reader = new SSTableReader(sst_file);

    // Test Constructor.

    // Correct path.
    EXPECT_EQ(sstable_reader->get_sst_path(), sst_file);

    // Index offsets must match, manually compute where the last block ends as expected_index_offset
    // and compare to SSTableReader member.
    uintmax_t expected_index_offset = written_entries.back().offset + written_entries.back().size;
    EXPECT_EQ(sstable_reader->get_index_offset(), expected_index_offset);

    // SSTableReader entries must match SSTableWriter entries.
    std::vector<IndexEntry> loaded_entries = sstable_reader->get_index_entries();
    ASSERT_EQ(loaded_entries.size(), written_entries.size());
    for (size_t i = 0; i < loaded_entries.size(); ++i) {
        EXPECT_EQ(loaded_entries[i].last_key, written_entries[i].last_key);
        EXPECT_EQ(loaded_entries[i].offset, written_entries[i].offset);
        EXPECT_EQ(loaded_entries[i].size, written_entries[i].size);
    }
}

// 2. Test Simple roundtrip operations.
// Put one value.
TEST_F(SSTableReaderTest, SimplePutRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test SSTable retrieves the value of the key with a FOUND lookup result.
    GetResult result = sstable_reader->get("key1");

    EXPECT_EQ(result.status, LookupResult::FOUND);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value.value(), "value1");
}

// Put one value and then delete.
TEST_F(SSTableReaderTest, SimplePutThenDeleteRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->remove("key1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test SSTable retrieves a DELETED lookup result, no value exists.
    GetResult result = sstable_reader->get("key1");

    EXPECT_EQ(result.status, LookupResult::DELETED);
    EXPECT_FALSE(result.value.has_value());
}

// Only Delete, tombstone still exists.
TEST_F(SSTableReaderTest, OnlyDeleteRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // key1 was never put or removed, however a tombstone record must exist.
    mem_table->remove("key1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test SSTable retrieves a DELETED lookup result, no value exists.
    GetResult result = sstable_reader->get("key1");

    EXPECT_EQ(result.status, LookupResult::DELETED);
    EXPECT_FALSE(result.value.has_value());
}

// Not found.
TEST_F(SSTableReaderTest, NotFoundRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test SSTable retrieves a NOT_FOUND lookup result, no value exists.
    GetResult result = sstable_reader->get("key99");

    EXPECT_EQ(result.status, LookupResult::NOT_FOUND);
    EXPECT_FALSE(result.value.has_value());
}

// Two puts, second put overwrites the first.
TEST_F(SSTableReaderTest, PutPutRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->put("key1", "value2");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test only the latest value survives.
    GetResult result = sstable_reader->get("key1");

    EXPECT_EQ(result.status, LookupResult::FOUND);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value.value(), "value2");

    // Only one record for key1 should exist in the file, confirms the
    // writer never wrote both.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    EXPECT_EQ(entries.size(), 1);
}

// Delete twice, overwritten with identical tombstone.
TEST_F(SSTableReaderTest, DeleteDeleteRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Second tombstone overwrites with identical tombstone.
    mem_table->remove("key1");
    mem_table->remove("key1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Test SSTable retrieves a DELETED lookup result, no value exists.
    GetResult result = sstable_reader->get("key1");

    EXPECT_EQ(result.status, LookupResult::DELETED);
    EXPECT_FALSE(result.value.has_value());
}

// Record resurrection.
TEST_F(SSTableReaderTest, PutDeletePutRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // key1 resurrects with a different value.
    mem_table->put("key1", "value1");
    mem_table->remove("key1");
    mem_table->put("key1", "value2");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    GetResult result = sstable_reader->get("key1");

    // Test SSTable retrieves a FOUND lookup result, with a resurrected value.
    EXPECT_EQ(result.status, LookupResult::FOUND);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value.value(), "value2");
}

// Add a tombstone, resurrect it and delete it again ending as tombstone.
TEST_F(SSTableReaderTest, DeletePutDeleteRoundTrip) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Key ends as tombstone.
    mem_table->remove("key1");
    mem_table->put("key1", "value1");
    mem_table->remove("key1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    GetResult result = sstable_reader->get("key1");

    // Test SSTable retrieves a DELETED lookup result, no value.
    EXPECT_EQ(result.status, LookupResult::DELETED);
    EXPECT_FALSE(result.value.has_value());

    // Only the final tombstone should exist in the file, no intermediate put.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    EXPECT_EQ(entries.size(), 1);
}

// 3. Test multiple blocks.
// 1K put operations.
TEST_F(SSTableReaderTest, RoundTripPutsAcrossMultipleBlocks) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Confirm the reader loaded the same number of blocks the writer produced.
    EXPECT_EQ(sstable_reader->get_index_entries().size(), entries.size());

    // Test each of the 1K records were found and has the correct value.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string expected_value = std::format("value{:03d}", i);

        GetResult result = sstable_reader->get(key);

        ASSERT_EQ(result.status, LookupResult::FOUND);
        ASSERT_TRUE(result.value.has_value());
        EXPECT_EQ(result.value.value(), expected_value);
    }
}

// 1K put operations and delete them immediately.
TEST_F(SSTableReaderTest, RoundTripPutsDeletesAcrossMultipleBlocks) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    // Remove all records.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        mem_table->remove(key);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Confirm the reader loaded the same number of blocks the writer produced.
    EXPECT_EQ(sstable_reader->get_index_entries().size(), entries.size());

    // Test each of the 1K records were DELETED and has the correct value.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string expected_value = std::format("value{:03d}", i);

        GetResult result = sstable_reader->get(key);

        ASSERT_EQ(result.status, LookupResult::DELETED);
        ASSERT_FALSE(result.value.has_value());
    }
}

// Put and Tombstone records at scale.
TEST_F(SSTableReaderTest, MixedPutAndTombstoneAtScaleRandomIntervals) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    // Delete keys at random intervals of 1-10.
    // Put a fixed seed so test has the same results each time.
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> gap(1, 10);

    // Track deleted indices in a set.
    std::set<int> deleted_indices;

    // First deletion at a random offset.
    int i = gap(rng);

    // Remove keys.
    while (i < total_records) {
        std::string key = std::format("key{:03d}", i);
        mem_table->remove(key);
        deleted_indices.insert(i);
        i += gap(rng);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Track tests works.
    int checked_found = 0;
    int checked_deleted = 0;

    // Iterate through all records.
    for (int j = 0; j < total_records; ++j) {
        std::string key = std::format("key{:03d}", j);
        GetResult result = sstable_reader->get(key);

        // If record was deleted it should not contain no value and be DELETED.
        if (deleted_indices.count(j)) {
            EXPECT_EQ(result.status, LookupResult::DELETED);
            EXPECT_FALSE(result.value.has_value());
            ++checked_deleted;
        } else {
            std::string expected_value = std::format("value{:03d}", j);
            ASSERT_EQ(result.status, LookupResult::FOUND);
            ASSERT_TRUE(result.value.has_value());
            EXPECT_EQ(result.value.value(), expected_value);
            ++checked_found;
        }
    }

    // Verify the test ran correctly and went through each of the records.
    EXPECT_GT(checked_deleted, 0);
    EXPECT_GT(checked_found, 0);
    EXPECT_EQ(checked_found + checked_deleted, total_records);
}

// Multiple Non-existent keys.
TEST_F(SSTableReaderTest, KeysNotFoundLookups) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Query 1000 keys that were never written, format missing000.
    // Values aren't found.
    for (int i = 0; i < total_records; ++i) {
        std::string missing_key = std::format("missing{:03d}", i);
        GetResult result = sstable_reader->get(missing_key);

        EXPECT_EQ(result.status, LookupResult::NOT_FOUND);
        EXPECT_FALSE(result.value.has_value());
    }
}

// 1K put operations, inserted in reverse order.
TEST_F(SSTableReaderTest, RoundTripPutsAcrossMultipleBlocksReverse) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    // Insert key999 down to key000 in reverse order to verify MemTable sorted records.
    for (int i = total_records - 1; i >= 0; --i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Confirm the reader loaded the same number of blocks the writer produced.
    EXPECT_EQ(sstable_reader->get_index_entries().size(), entries.size());

    // Test each of the 1K records were found and has the correct value, proving sorted order.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string expected_value = std::format("value{:03d}", i);

        GetResult result = sstable_reader->get(key);

        ASSERT_EQ(result.status, LookupResult::FOUND);
        ASSERT_TRUE(result.value.has_value());
        EXPECT_EQ(result.value.value(), expected_value);
    }
}

// All indexes boundary keys get written.
TEST_F(SSTableReaderTest, BoundaryKeyLookups) {

    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm multiple blocks were built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 1);

    // Instantiate SSTableReader.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    sstable_reader = new SSTableReader(sst_file);

    // Query every block's exact last_key.
    for (const auto& entry : entries) {
        GetResult result = sstable_reader->get(entry.last_key);

        EXPECT_EQ(result.status, LookupResult::FOUND);
        ASSERT_TRUE(result.value.has_value());

        // Derive the expected value from the key itself.
        std::string index_str = entry.last_key.substr(3);
        std::string expected_value = "value" + index_str;
        EXPECT_EQ(result.value.value(), expected_value);
    }
}

// 4. Test Corruption
// Corrupt a one-block file.
TEST_F(SSTableReaderTest, CorruptedBlockThrowsOnGet) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    mem_table->put("key3", "value3");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Confirm one block was built.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_EQ(entries.size(), 1);

    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";

    // Corrupt a byte in the middle of the block's data.
    uintmax_t data_len = entries[0].size - sizeof(uint32_t);
    uintmax_t deep_offset = entries[0].offset + (data_len / 2);
    corrupt_byte_at(sst_file, deep_offset);

    sstable_reader = new SSTableReader(sst_file);

    // The corruption is inside the block, runtime_error is thrown.
    EXPECT_THROW(sstable_reader->get("key2"), std::runtime_error);
}

// Corrupt second block instead.
TEST_F(SSTableReaderTest, CorruptedSecondBlock) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Write 1K distinct kv-pairs with the format key001: value001, ...
    int total_records = 1000;

    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value = std::format("value{:03d}", i);
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // More than two blocks need to exist.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_GT(entries.size(), 2);

    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";

    // Corrupt a byte in the middle of the second block's data specifically,
    // confirming detection isn't tied to first read block.
    const IndexEntry& second_block = entries[1];
    uintmax_t data_len = second_block.size - sizeof(uint32_t);
    uintmax_t deep_offset = second_block.offset + (data_len / 2);
    corrupt_byte_at(sst_file, deep_offset);

    sstable_reader = new SSTableReader(sst_file);

    // The corruption is inside the block, runtime_error is thrown.
    EXPECT_THROW(sstable_reader->get(second_block.last_key), std::runtime_error);

    // Sanity check: a key in the first block still works.
    GetResult first_block_result = sstable_reader->get(entries[0].last_key);
    EXPECT_EQ(first_block_result.status, LookupResult::FOUND);
}

// Corrupt Magic Number.
TEST_F(SSTableReaderTest, CorruptedMagicNumber) {
    // Set up SSTableWriter and MemTable for test.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Test file was correctly set up.
    std::filesystem::path sst_file = std::filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Corruption inside the 4-byte magic field.
    uintmax_t file_size = std::filesystem::file_size(sst_file);
    uintmax_t magic_offset = file_size - sizeof(uint32_t) / 2;
    corrupt_byte_at(sst_file, magic_offset);

    // Throw happens during construction at load_footer().
    EXPECT_THROW(sstable_reader = new SSTableReader(sst_file), std::runtime_error);
}



