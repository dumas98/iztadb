//
// Created by Daniel Dumas on 11/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"
#include "types.h"

using namespace std;

/**
 * @brief Test fixture for MemTable.
 * Builds a LogWriter instance before each test, adds directories and removes them after each test.
 * Experiments with different WAL file sizes to guarantees rotation.
 *
 */
class LogWriterTest : public testing::Test {
    protected:
    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    LogWriter* log_writer1;

    // Before Each.
    void SetUp() override {
        // Build test directory.
        std::filesystem::create_directories("./test_data/wal");

        // One record per file.
        log_writer1= new LogWriter("./test_data/wal", sizeof(WALRecord));
    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete log_writer1;
        log_writer1 = nullptr;

        // Drop test directory.
        std::filesystem::remove_all("./test_data/wal");
    }
};

// Verifies Default Constructor variables are assigned correctly.
// Only test wal_path and file size.
TEST_F(LogWriterTest, DefaultConstructorInitializesCorrectly) {
    LogWriter default_log_writer;
    EXPECT_EQ(default_log_writer.get_wal_path(), "./data/wal");
    EXPECT_EQ(default_log_writer.get_max_wal_file_size(), 32 * 1024 * 1024);
}

// Test each attribute from non-default constructor.
TEST_F(LogWriterTest, NonDefaultConstructorInitializesCorrectly) {
    EXPECT_EQ(log_writer1->get_wal_path(), "./test_data/wal");
    EXPECT_EQ(log_writer1->get_max_wal_file_size(), sizeof(WALRecord));
    EXPECT_EQ(log_writer1->get_next_sequence(), 1);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 0);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal/000001.log");
}

// Test writing one record and attributes correctly work
TEST_F(LogWriterTest, WriteOneRecordCorrectAttributes) {
    // Add one record.
    log_writer1->write_record("username", "daniel", ValueType::VALUE);

    EXPECT_EQ(log_writer1->get_wal_path(), "./test_data/wal");
    EXPECT_EQ(log_writer1->get_max_wal_file_size(), sizeof(WALRecord));
    EXPECT_EQ(log_writer1->get_next_sequence(), 2);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 1);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal/000002.log");
}

// Test writing one record and attributes correctly work
TEST_F(LogWriterTest, WriteMultipleRecordsCorrectAttributes) {
    // Add eleven records, do half with VALUE and half with TOMBSTONE ValueTypes.
    log_writer1->write_record("daniel", "dumas", ValueType::VALUE);
    log_writer1->write_record("taylor", "swift", ValueType::VALUE);
    log_writer1->write_record("serena", "williams", ValueType::VALUE);
    log_writer1->write_record("roger", "federer", ValueType::VALUE);
    log_writer1->write_record("tiger", "woods", ValueType::VALUE);
    log_writer1->write_record("katie", "perry", ValueType::VALUE);
    log_writer1->write_record("hannah", "montana", ValueType::TOMBSTONE);
    log_writer1->write_record("luke", "skywalker", ValueType::TOMBSTONE);
    log_writer1->write_record("leia", "skywalker", ValueType::TOMBSTONE);
    log_writer1->write_record("freddie", "mercury", ValueType::TOMBSTONE);
    log_writer1->write_record("leonardo", "dicaprio", ValueType::TOMBSTONE);

    EXPECT_EQ(log_writer1->get_wal_path(), "./test_data/wal");
    EXPECT_EQ(log_writer1->get_max_wal_file_size(), sizeof(WALRecord));
    EXPECT_EQ(log_writer1->get_next_sequence(), 12);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 11);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal/000012.log");
}

