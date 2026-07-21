//
// Created by Daniel Dumas on 21/07/26.
//

#include <gtest/gtest.h>
#include "memtable.h"
#include "sstable_utils.h"
#include "sstable_writer.h"
#include "sstable_reader.h"

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



