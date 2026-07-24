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

    // Write path should be in current file.
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

// Multiple Tombstone Writes.
TEST_F(SSTableWriterTest, MultipleTombstoneWrites) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Iterator.
    int iterator = 8;

    // Add data to MemTable first, then remove it, only tombstones
    // keys that currently exist.
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        string value = "value" + to_string(i);
        mem_table->put(key, value);
    }
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        mem_table->remove(key);
    }

    // Add one last record where it only was removed and never existed before.
    mem_table->remove("key9");

    // Check tombstones were written to MemTable.
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        EXPECT_EQ(mem_table->get(key), std::nullopt);
    }

    // Flush MemTable, check it was created.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file.
    ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Retrieve Records.
    for (int i = 1; i <= iterator+1; ++i) {
        // Expected Variables
        string exp_key = "key" + to_string(i);
        uint32_t exp_key_len = 4;

        // Tombstones carry no value bytes.
        uint32_t exp_value_len = 0;

        uint32_t key_len;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        EXPECT_EQ(key_len, exp_key_len);

        string key(key_len, '\0');
        in.read(key.data(), key_len);
        EXPECT_EQ(key, exp_key);

        uint8_t type;
        in.read(reinterpret_cast<char*>(&type), sizeof(type));
        EXPECT_EQ(static_cast<ValueType>(type), ValueType::TOMBSTONE);

        uint32_t value_len;
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        EXPECT_EQ(value_len, exp_value_len);

    }
}

// Mixed Put and Tombstone Writes.
TEST_F(SSTableWriterTest, MixedPutAndTombstoneWrites) {
    // Set up for tests.
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    int iterator = 9;

    // Put every key first.
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        string value = "value" + to_string(i);
        mem_table->put(key, value);
    }

    // Tombstone every other key.
    for (int i = 2; i <= iterator; i += 2) {
        string key = "key" + to_string(i);
        mem_table->remove(key);
    }

    // Test MemTable was correctly populated.
    for (int i = 1; i <= iterator; ++i) {
        string key = "key" + to_string(i);
        if (i % 2 == 0) {
            EXPECT_EQ(mem_table->get(key), std::nullopt);
        } else {
            EXPECT_EQ(mem_table->get(key), "value" + to_string(i));
        }
    }

    // Flush, check file was created.
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Sorted order of keys.
    std::vector<std::string> expected_order = {
        "key1", "key2", "key3", "key4", "key5",
        "key6", "key7", "key8", "key9"
    };

    for (const auto& exp_key : expected_order) {
        // Even numbers are Tombstone.
        int i = std::stoi(exp_key.substr(3));
        bool is_tombstone = (i % 2 == 0);

        // Key size is the same for all.
        uint32_t key_len;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        EXPECT_EQ(key_len, exp_key.size());

        // Test key follows sorted order.
        string key(key_len, '\0');
        in.read(key.data(), key_len);
        EXPECT_EQ(key, exp_key);

        // Type and value len is the same.
        uint8_t type;
        in.read(reinterpret_cast<char*>(&type), sizeof(type));

        uint32_t value_len;
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));

        // Tombstones have a 0 value len and values follow PUT logic from previous tests.
        if (is_tombstone) {
            EXPECT_EQ(static_cast<ValueType>(type), ValueType::TOMBSTONE);
            EXPECT_EQ(value_len, 0);
        } else {
            EXPECT_EQ(static_cast<ValueType>(type), ValueType::VALUE);
            string exp_value = "value" + to_string(i);
            EXPECT_EQ(value_len, exp_value.size());
            string value(value_len, '\0');
            in.read(value.data(), value_len);
            EXPECT_EQ(value, exp_value);
        }
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

// 4. Block Boundaries via indexes.
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

    // Test the first and last keys on disk match the sorted extremes.
    EXPECT_EQ(entries.front().offset, 0);
    EXPECT_EQ(entries.back().last_key, "key099");
}

TEST_F(SSTableWriterTest, TombstonesRespectBlockBoundaries) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    int total_records = 100;
    // Same logic key000, key001
    uint32_t key_len = 6;

    // Non-tombstone records are 50 as well
    uint32_t value_len = 50;
    uint32_t crc_size = sizeof(uint32_t);

    // Same logic as previous test, 65.
    // Tombstone is 15, missing the x*50 part.
    uint32_t put_record_size = sizeof(uint32_t) + key_len + sizeof(uint8_t) + sizeof(uint32_t) + value_len;   // 65
    uint32_t tombstone_record_size = sizeof(uint32_t) + key_len + sizeof(uint8_t) + sizeof(uint32_t);

    // Build MemTable: put every key, then tombstone every 5th.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        mem_table->put(key, std::string(value_len, 'x'));
    }
    for (int i = 0; i < total_records; i += 5) {
        mem_table->remove(std::format("key{:03d}", i));
    }

    // Build a list of structs of what's the expected index.
    struct ExpectedBlock { uintmax_t offset; uintmax_t size; std::string last_key; };
    std::vector<ExpectedBlock> expected_blocks;

    uintmax_t running_offset = 0;
    uint32_t running_size = 0;
    std::string last_key;

    // Build expected indexes.
    for (int i = 0; i < total_records; ++i) {
        std::string key = std::format("key{:03d}", i);
        bool is_tombstone = (i % 5 == 0);

        // running_size depends if record is a tombstone.
        running_size += is_tombstone ? tombstone_record_size : put_record_size;
        last_key = key;

        // Only register after it passed the block threshold, add block size to offset and restart
        // block size.
        if (running_size >= 1024) {
            running_size += crc_size;
            expected_blocks.push_back({running_offset, running_size, last_key});
            running_offset += running_size;
            running_size = 0;
        }
    }

    // Last block, add remaining index.
    if (running_size > 0) {
        running_size += crc_size;
        expected_blocks.push_back({running_offset, running_size, last_key});
    }

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();

    // Compare actual indexes size vs. expected.
    ASSERT_EQ(entries.size(), expected_blocks.size());
    for (size_t b = 0; b < entries.size(); ++b) {
        EXPECT_EQ(entries[b].offset, expected_blocks[b].offset);
        EXPECT_EQ(entries[b].size, expected_blocks[b].size);
        EXPECT_EQ(entries[b].last_key, expected_blocks[b].last_key);
    }

    // Same as ordering records, the previous key is less that the next key to check ordering.
    // Check correct offsets.
    for (size_t b = 0; b + 1 < entries.size(); ++b) {
        EXPECT_LT(entries[b].last_key, entries[b + 1].last_key);
        EXPECT_EQ(entries[b].offset + entries[b].size, entries[b + 1].offset);
    }
}

// 5. Test Checksum
// Test it's working correctly for a single block.
TEST_F(SSTableWriterTest, BlockChecksumIsValid)
{
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Insert values to MemTable.
    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    mem_table->put("key3", "value3");

    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    // Return indexes.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_EQ(entries.size(), 1);

    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    std::ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Block is first entry.
    const IndexEntry& block = entries[0];

    // Build a buffer with block in a vector of bytes.
    std::vector<uint8_t> block_bytes(block.size);
    in.seekg(block.offset);
    in.read(reinterpret_cast<char*>(block_bytes.data()), block.size);
    ASSERT_TRUE(in.good());

    // Extract calculated CRC from block.
    uint32_t stored_crc;
    std::memcpy(&stored_crc, block_bytes.data() + block.size - sizeof(uint32_t), sizeof(uint32_t));

    // Extract data portion and recalculate CRC.
    std::vector<uint8_t> data_only(block_bytes.begin(), block_bytes.end() - sizeof(uint32_t));
    uint32_t recomputed_crc = iztadb::sstable::calculate_crc32(data_only);

    // Both numbers must match.
    EXPECT_EQ(stored_crc, recomputed_crc);
}

// Corrupt a block and checksum verification works.
TEST_F(SSTableWriterTest, CorruptedBlockFailsChecksumVerification) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    mem_table->put("key3", "value3");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_EQ(entries.size(), 1);

    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";

    // Record size minus checksum.
    uintmax_t data_len = entries[0].size - sizeof(uint32_t);

    // Corrupt a byte middle of file.
    uintmax_t offset = entries[0].offset + (data_len / 2);
    corrupt_byte_at(sst_file, offset);

    // Open file.
    std::ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Get index for entry 0.
    const IndexEntry& block = entries[0];

    // Read block bytes to vector buffer.
    std::vector<uint8_t> block_bytes(block.size);
    in.seekg(block.offset);
    in.read(reinterpret_cast<char*>(block_bytes.data()), block.size);
    ASSERT_TRUE(in.good());

    // Extract Checksum from original record.
    uint32_t stored_crc;
    std::memcpy(&stored_crc, block_bytes.data() + block.size - sizeof(uint32_t), sizeof(uint32_t));

    // Extract Corrupted data and recompute checksum, checksums dont match.
    std::vector<uint8_t> data_only(block_bytes.begin(), block_bytes.end() - sizeof(uint32_t));
    uint32_t recomputed_crc = iztadb::sstable::calculate_crc32(data_only);

    // Checksums don't match.
    EXPECT_NE(stored_crc, recomputed_crc);
}

// 6. Test Footer.
TEST_F(SSTableWriterTest, FooterMagicNumberIsCorrect) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Insert data to MemTable.
    mem_table->put("key1", "value1");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file.
    std::ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // File size is greater than footer.
    uintmax_t file_size = std::filesystem::file_size(sst_file);
    ASSERT_GE(file_size, iztadb::sstable::kFooterSize);

    // Go to footer.
    in.seekg(file_size - iztadb::sstable::kFooterSize);

    // Pass index offset.
    uintmax_t index_offset;
    in.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    // Read magic and check it matches with actual magic.
    uint32_t magic;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    EXPECT_EQ(magic, iztadb::sstable::kMagic);
}

// Retrieve Footer Index and validate index.
TEST_F(SSTableWriterTest, FooterIndexOffsetPointsToValidIndex) {
    sstable_writer_1kb = new SSTableWriter(sst_path_1kb, 1024);

    // Insert data to MemTable.
    mem_table->put("key1", "value1");
    mem_table->put("key2", "value2");
    ASSERT_TRUE(sstable_writer_1kb->flush_mem_table(*mem_table));

    filesystem::path sst_file = filesystem::path(sst_path_1kb) / "000001.sst";
    ASSERT_TRUE(std::filesystem::exists(sst_file));

    // Open file.
    std::ifstream in(sst_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    // Use file size to seek for footer.
    uintmax_t file_size = std::filesystem::file_size(sst_file);
    in.seekg(file_size - iztadb::sstable::kFooterSize);

    // Extract index_offset (where indexes start).
    uintmax_t index_offset;
    in.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    // The index should start exactly where the last data block ended.
    std::vector<IndexEntry> entries = sstable_writer_1kb->get_index_entries();
    ASSERT_FALSE(entries.empty());
    uintmax_t expected_index_offset = entries.back().offset + entries.back().size;

    EXPECT_EQ(index_offset, expected_index_offset);

    // Read all indexes entries and match vs. actual index values, they must match.
    in.seekg(index_offset);
    for (const auto& entry : entries) {
        uint32_t key_len;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        EXPECT_EQ(key_len, entry.last_key.size());

        std::string key(key_len, '\0');
        in.read(key.data(), key_len);
        EXPECT_EQ(key, entry.last_key);

        uintmax_t offset;
        in.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        EXPECT_EQ(offset, entry.offset);

        uint32_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        EXPECT_EQ(size, entry.size);
    }

    // After reading all indexes read cursor must be before footer.
    EXPECT_EQ(in.tellg(), static_cast<std::streampos>(file_size - iztadb::sstable::kFooterSize));
}










