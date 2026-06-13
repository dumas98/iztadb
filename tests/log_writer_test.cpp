//
// Created by Daniel Dumas on 11/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"
#include "types.h"

using namespace std;

/**
 * @brief Test fixture for LogWriter.
 * Builds a LogWriter instance before each test, adds directories and removes them after each test.
 * Experiments with different WAL file sizes to guarantees rotation.
 *
 */
class LogWriterTest : public testing::Test {
    protected:
    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    LogWriter* log_writer1;
    LogWriter* log_writer_half;
    LogWriter* log_writer1_half;
    LogWriter* log_writer2;
    LogWriter* log_writer_32mb;

    // Before Each.
    void SetUp() override {
        // Build test directories.

        std::filesystem::create_directories("./test_data/wal_32mb");
        std::filesystem::create_directories("./test_data/wal1");
        std::filesystem::create_directories("./test_data/wal_half");
        std::filesystem::create_directories("./test_data/wal1_half");
        std::filesystem::create_directories("./test_data/wal2");

        // Experiment with different wal file size thresholds.
        log_writer1 = new LogWriter("./test_data/wal1", sizeof(WALRecord));
        log_writer_half = new LogWriter("./test_data/wal_half", sizeof(WALRecord) / 2);
        log_writer1_half = new LogWriter("./test_data/wal1_half",
                                         static_cast<uintmax_t>(sizeof(WALRecord) * 1.5));
        log_writer2 = new LogWriter("./test_data/wal2", sizeof(WALRecord) * 2);
        log_writer_32mb = new LogWriter("./test_data/wal_32mb", 32 * 1024 * 1024);
    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete log_writer1;
        delete log_writer_half;
        delete log_writer1_half;
        delete log_writer2;
        delete log_writer_32mb;

        // Drop test directory.
        std::filesystem::remove_all("./test_data/wal");
        std::filesystem::remove_all("./test_data/wal1");
        std::filesystem::remove_all("./test_data/wal_half");
        std::filesystem::remove_all("./test_data/wal1_half");
        std::filesystem::remove_all("./test_data/wal2");
        std::filesystem::remove_all("./test_data/wal_32mb");
    }

    // Count log files in a directory.
    static int count_files(const std::string& path) {
        int count = 0;
        for (auto const& entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() == ".log") count++;
        }
        return count;
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

// Rotation should happen after one record since max_size equals exactly one record size.
TEST_F(LogWriterTest, RotationAtExactRecordSize) {
    log_writer1->write_record("username", "daniel", ValueType::VALUE);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal1/000002.log");
    EXPECT_EQ(log_writer1->get_next_sequence(), 2);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 1);
}

// Even when threshold is below record size, rotation behavior is identical to exact size.
// Record is always written regardless of threshold being smaller than record.
TEST_F(LogWriterTest, RotationWhenThresholdBelowRecordSize) {
    log_writer_half->write_record("username", "daniel", ValueType::VALUE);
    EXPECT_EQ(log_writer_half->get_write_path(), "./test_data/wal_half/000002.log");
    EXPECT_EQ(log_writer_half->get_next_sequence(), 2);
    EXPECT_EQ(log_writer_half->get_latest_segment_num(), 1);
}

// No rotation expected since threshold is 1.5x record size.
// File can still accommodate more records before rotating.
TEST_F(LogWriterTest, NoRotationWhenThresholdAboveRecordSize) {
    log_writer1_half->write_record("username", "daniel", ValueType::VALUE);
    EXPECT_EQ(log_writer1_half->get_write_path(), "./test_data/wal1_half/000001.log");
    EXPECT_EQ(log_writer1_half->get_next_sequence(), 2);
    EXPECT_EQ(log_writer1_half->get_latest_segment_num(), 1);
}

// No rotation expected since threshold fits exactly two records.
// After one record file is still under threshold.
TEST_F(LogWriterTest, NoRotationWithTwoRecordsPerFile) {
    log_writer2->write_record("username", "daniel", ValueType::VALUE);
    EXPECT_EQ(log_writer2->get_write_path(), "./test_data/wal2/000001.log");
    EXPECT_EQ(log_writer2->get_next_sequence(), 2);
    EXPECT_EQ(log_writer2->get_latest_segment_num(), 1);
}

// Test writing multiple records, attributes and file count match rotations.
TEST_F(LogWriterTest, WriteMultipleRecordsCorrectAttributes) {
    // Insert 2026 records alternating VALUE and TOMBSTONE.
    for (int i = 1; i <= 2026; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        ValueType type;

        // Odd numbers get VALUE, even numbers get TOMBSTONE.
        if (i % 2 == 0) {
            type = ValueType::TOMBSTONE;
        } else {
            type = ValueType::VALUE;
        }

        log_writer1->write_record(key, value, type);
        log_writer_half->write_record(key, value, type);
        log_writer1_half->write_record(key, value, type);
        log_writer2->write_record(key, value, type);
    }

    // One record per file -> 2026 files, next path is 002027.log.
    EXPECT_EQ(count_files("./test_data/wal1"), 2026);
    EXPECT_EQ(log_writer1->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 2026);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal1/002027.log");

    // Below record size -> same as one record per file.
    EXPECT_EQ(count_files("./test_data/wal_half"), 2026);
    EXPECT_EQ(log_writer_half->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer_half->get_latest_segment_num(), 2026);
    EXPECT_EQ(log_writer_half->get_write_path(), "./test_data/wal_half/002027.log");

    // 1.5x record size -> two records per file since one record isn't enough for threshold.
    EXPECT_EQ(count_files("./test_data/wal1_half"), 1013);
    EXPECT_EQ(log_writer1_half->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer1_half->get_latest_segment_num(), 1013);
    EXPECT_EQ(log_writer1_half->get_write_path(), "./test_data/wal1_half/001014.log");

    // Two records per file -> 1013 files.
    EXPECT_EQ(count_files("./test_data/wal2"), 1013);
    EXPECT_EQ(log_writer2->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer2->get_latest_segment_num(), 1013);
    EXPECT_EQ(log_writer2->get_write_path(), "./test_data/wal2/001014.log");
}

// Test actual 32MB threshold with 85K records.
TEST_F(LogWriterTest, RotationWith32MBThreshold) {
    LogWriter* log_writer_32mb = new LogWriter("./test_data/wal_32mb", 32 * 1024 * 1024);
    std::filesystem::create_directories("./test_data/wal_32mb");

    // Insert enough records to trigger exactly one rotation.
    for (int i = 1; i <= 85381; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        if (i % 2 == 0) {
            log_writer_32mb->write_record(key, value, ValueType::TOMBSTONE);
        } else {
            log_writer_32mb->write_record(key, value, ValueType::VALUE);
        }
    }

    EXPECT_EQ(count_files("./test_data/wal_32mb"), 1);
    EXPECT_EQ(log_writer_32mb->get_latest_segment_num(), 1);
    EXPECT_EQ(log_writer_32mb->get_write_path(), "./test_data/wal_32mb/000002.log");
    EXPECT_EQ(log_writer_32mb->get_next_sequence(), 85382);
}