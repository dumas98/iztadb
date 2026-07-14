//
// Created by Daniel Dumas on 07/07/26.
//

#include <gtest/gtest.h>
#include "memtable.h"
#include "sstable_utils.h"
#include "sstable_writer.h"

using namespace std;

/**
 * @brief Test fixture for SSTableWriter.
 * Builds a SSTableWriter instance before each test, adds directories and removes them after each test.
 *
 */
class SSTableWriterTest : public testing::Test{
protected:
    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    // Variable structure: sstable_writer_{block size}
    SSTableWriter* sstable_writer_1kb;
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

        // Drop test directory.
        std::filesystem::remove_all("./test_data/");
        std::cout << "Test Ended." << std::endl;
    }
};

// 1. Test constructors

// Verifies default constructor attributes are assigned correctly.
TEST_F(SSTableWriterTest, DefaultConstructorInitializesCorrectly) {
    SSTableWriter default_sstable_writer;
    EXPECT_EQ(default_sstable_writer.get_sstable_path(), "./data/sst");
    EXPECT_EQ(default_sstable_writer.get_block_size(), 4096);
}

// Test each attribute from non-default constructor, empty directory.
TEST_F(SSTableWriterTest, NonDefaultConstructorEmptyDir) {

    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    EXPECT_EQ(sstable_writer_1kb->get_sstable_path(), sst_path_1kb);
    EXPECT_EQ(sstable_writer_1kb->get_block_size(), 1024);
    EXPECT_EQ(sstable_writer_1kb->get_latest_segment_num(), 0);
    EXPECT_EQ(sstable_writer_1kb->get_write_path(), sst_path_1kb + "/000001.sst");
}

// Test each attribute from non-default constructor, non-empty directory.
TEST_F(SSTableWriterTest, NonDefaultConstructorNonEmptyDir) {

    // Build 10 sst empty files.
    for (int i = 1; i <= 10; ++i) {
        filesystem::path existing = filesystem::path(sst_path_1kb) / format("{:06d}.sst", i);
        ofstream(existing).close();
    }
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Verify correct write path on non-empty directory.
    EXPECT_EQ(sstable_writer_1kb->get_latest_segment_num(), 10);
    EXPECT_EQ(sstable_writer_1kb->get_write_path(), sst_path_1kb + "/000011.sst");
}

// 2. Basic File Writing
// Check flush_mem_table returns true if successful.
TEST_F(SSTableWriterTest, ReturnsTrueOnSuccessfulFlush) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);
    mem_table->put("key1", "value1");
    EXPECT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
}

// Check flush_mem_table creates a non-empty file.
TEST_F(SSTableWriterTest, FileIsNonEmptyAfterFlush) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);
    mem_table->put("key1", "value1");

    // Check MemTable flushed.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Check it created a file.
    filesystem::path expected_sst = filesystem::path(sst_path_1kb) / "000001.sst";
    EXPECT_TRUE(filesystem::exists(expected_sst));

    // Check file is non-empty.
    EXPECT_GT(filesystem::file_size(expected_sst), 0);
}

TEST_F(SSTableWriterTest, RebuildsMemTableAfterFlush) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);
    mem_table->put("key1", "value1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Rebuild MemTable and check it's empty.
    delete mem_table;
    mem_table = new MemTable();
    EXPECT_EQ(mem_table->get("key1"), nullopt);

    // New writes land in the fresh MemTable, independent of the old one.
    mem_table->put("key2", "value2");
    EXPECT_EQ(mem_table->get("key2"), "value2");

    // Flush again MemTable.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // The first flush's data got flushed.
    filesystem::path first_sst = filesystem::path(sst_path_1kb) / "000001.sst";
    EXPECT_TRUE(filesystem::exists(first_sst));
    EXPECT_GT(filesystem::file_size(first_sst), 0);

    // The second flush's data got flushed.
    filesystem::path second_sst = filesystem::path(sst_path_1kb) / "000002.sst";
    EXPECT_TRUE(filesystem::exists(second_sst));
    EXPECT_GT(filesystem::file_size(second_sst), 0);

    // Write path should be in the next file.
    EXPECT_EQ(sstable_writer_1kb->get_write_path(), sst_path_1kb + "/000003.sst");
}

// 3. Record Layout.
// Single Write
TEST_F(SSTableWriterTest, SingleRecordWrite) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);
    mem_table->put("key1", "value1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // File was created.
    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file
    ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Read the record directly from the start of the first block.
    uint32_t key_len;
    in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
    EXPECT_EQ(key_len, 4);

    string key(key_len, '\0');
    in.read(key.data(), key_len);
    EXPECT_EQ(key, "key1");

    uint8_t type;
    in.read(reinterpret_cast<char*>(&type), sizeof(type));
    EXPECT_EQ(static_cast<ValueType>(type), ValueType::VALUE);

    uint32_t value_len;
    in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
    EXPECT_EQ(value_len, 6);

    string value(value_len, '\0');
    in.read(value.data(), value_len);
    EXPECT_EQ(value, "value1");
}

// Multiple Writes.
TEST_F(SSTableWriterTest, MultipleRecordWrites) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Iterator
    int iterator = 9;

    // Add data to MemTable.
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        string value = "value" + to_string(i);
        mem_table->put(key, value);
    }

    // Check data was added to MemTable
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        string value = "value" + to_string(i);
        ASSERT_EQ(mem_table->get(key), value);
    }

    // Flush MemTable, check it was created.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file.
    ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Retrieve Records.
    for (int i = 1; i <= iterator; ++i) {
        // Expected Variables
        string exp_key = "key" + to_string(i);
        string exp_value = "value" + to_string(i);
        uint32_t exp_key_len = 4;
        uint32_t exp_value_len = 6;

        uint32_t key_len;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        EXPECT_EQ(key_len, exp_key_len);

        string key(key_len, '\0');
        in.read(key.data(), key_len);
        EXPECT_EQ(key, exp_key);

        uint8_t type;
        in.read(reinterpret_cast<char*>(&type), sizeof(type));
        EXPECT_EQ(static_cast<ValueType>(type), ValueType::VALUE);

        uint32_t value_len;
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        EXPECT_EQ(value_len, exp_value_len);

        string value(value_len, '\0');
        in.read(value.data(), value_len);
        EXPECT_EQ(value, exp_value);

    }

}

// Check SSTable block is in correct order.
TEST_F(SSTableWriterTest, OrdersRecordsRegardlessOfInsertionOrder) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Insert Z down to A reverse order.
    for (char c = 'Z'; c >= 'A'; --c) {
        std::string key(1, c);
        std::string value = "value_" + key;
        mem_table->put(key, value);
    }

    // Flush MemTable, check it was created.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file.
    ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // 26 single letter keys and short values are well under 1KB combined.
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string exp_key(1, c);
        std::string exp_value = "value_" + exp_key;

        uint32_t key_len;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        ASSERT_EQ(key_len, exp_key.size());

        std::string key(key_len, '\0');
        in.read(key.data(), key_len);
        EXPECT_EQ(key, exp_key);

        uint8_t type;
        in.read(reinterpret_cast<char*>(&type), sizeof(type));
        EXPECT_EQ(static_cast<ValueType>(type), ValueType::VALUE);

        uint32_t value_len;
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        ASSERT_EQ(value_len, exp_value.size());

        std::string value(value_len, '\0');
        in.read(value.data(), value_len);
        EXPECT_EQ(value, exp_value);
    }
}

// 4. Index Layouts
TEST_F(SSTableWriterTest, IndexEntriesMatchExactBlockBoundaries) {

    // Fixed sized records.
    uint32_t key_len = 6;
    uint32_t value_len = 50;
    // Record size: 4+6+1+4+50 = 65
    uint32_t record_size =
        sizeof(uint32_t) + key_len + sizeof(uint8_t) + sizeof(uint32_t) + value_len;

    uint32_t crc_size = sizeof(uint32_t);
    uintmax_t block_size_local = 1024;
    int total_records = 100;

    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, block_size_local);

    // Insert the 100 records each the same
    // Key key001, key002, ... and value always x*50
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        std::string value(value_len, 'x');
        mem_table->put(key, value);
    }

    // Flush MemTable and get index entries.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();

    // Calculate records per block with CEIL(1024 / 65) = 16
    uint32_t records_per_block =
        static_cast<uint32_t>((block_size_local + record_size - 1) / record_size);

    // Block size (actual after threshold). 16*65 + 4 = 1044
    uint32_t full_block_size = records_per_block * record_size + crc_size;

    // Integer division 100/16 = 6, remainder = 4
    int num_full_blocks = total_records / records_per_block;
    int remainder = total_records % records_per_block;

    // Block count are full blocks plus one if remainder is greater than 0.
    int expected_block_count = num_full_blocks + (remainder > 0 ? 1 : 0);  // 7

    // Block count must match number of indexes.
    ASSERT_EQ(entries.size(), static_cast<size_t>(expected_block_count));

    // Iterate through each index (Only full blocks).
    // Offset starts at zero and record index is last record of block.
    uintmax_t expected_offset = 0;
    int record_index = 0;
    for (int b = 0; b < num_full_blocks; ++b) {

        // Check each offset and constant size.
        EXPECT_EQ(entries[b].offset, expected_offset);
        EXPECT_EQ(entries[b].size, full_block_size);

        // Add records per block and subtract the extra because it starts at key000.
        record_index += records_per_block;
        std::string expected_last_key = std::format("key{:03d}", record_index - 1);
        EXPECT_EQ(entries[b].last_key, expected_last_key);

        // Add block size as new offset.
        expected_offset += full_block_size;
    }

    // Check last record if it was partial.
    if (remainder > 0) {
        uint32_t last_block_size = remainder * record_size + crc_size;  // 264
        EXPECT_EQ(entries.back().offset, expected_offset);
        EXPECT_EQ(entries.back().size, last_block_size);
        EXPECT_EQ(entries.back().last_key, std::format("key{:03d}", total_records - 1));
    }
}

// Orders records across multiple blocks.
TEST_F(SSTableWriterTest, OrdersRecordsAcrossMultipleBlocks) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Insert key099 down to key000 with values large enough that 100 records span several blocks,
    // not just one.
    for (int i = 99; i >= 0; --i) {
        std::string key = std::format("key{:03d}", i);
        std::string value(50, 'x');
        mem_table->put(key, value);
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    // Confirm this genuinely spans multiple blocks.
    ASSERT_GT(entries.size(), 1);

    // last_key should be in increasing order always larger than next block last key.
    for (size_t b = 0; b + 1 < entries.size(); ++b) {
        EXPECT_LT(entries[b].last_key, entries[b + 1].last_key);
    }

    // Test the  first and  last keys on disk match the sorted extremes.
    EXPECT_EQ(entries.front().offset, 0);
    EXPECT_EQ(entries.back().last_key, "key099");
}











