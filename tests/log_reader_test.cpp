//
// Created by Daniel Dumas on 27/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"
#include "log_reader.h"
#include "memtable.h"
#include "log_utils.h"
#include "types.h"

/**
 * @brief Test fixture for LogReader.
 * Builds a LogReader a LogWriter and a MemTable instance before each test,
 * adds directories and removes them after each test.
 *
 */
class LogReaderTest : public testing::Test
{
protected:

    std::string wal_path = "./test_data/wal";
    std::string wal_path4 = "./test_data/wal4";

    ValueType type_value = ValueType::VALUE;
    ValueType type_tombstone = ValueType::TOMBSTONE;

    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    LogWriter* log_writer;
    LogWriter* log_writer4;
    MemTable* mem_table;

    // Before Each.
    void SetUp() override {
        // Build test directories.

        std::filesystem::create_directories(wal_path);
        std::filesystem::create_directories(wal_path4);

        // Instantiate LogWriter, LogReader and MemTable
        log_writer = new LogWriter(wal_path);
        log_writer4 = new LogWriter(wal_path4, sizeof(WALRecord) * 4);
        mem_table = new MemTable();
    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete log_writer;
        delete log_writer4;
        delete mem_table;

        // Drop test directory.
        std::filesystem::remove_all(wal_path);
        std::filesystem::remove_all(wal_path4);
    }

    // Write sample records to WAL.
    void write_sample_records(int number_of_writes, LogWriter* log_writer) {
        for (int i = 1; i <= number_of_writes; i++) {
            std::string key = "key" + std::to_string(i);
            std::string value = "value" + std::to_string(i);
            ValueType type;

            // Odd numbers get VALUE, even numbers get TOMBSTONE.
            if (i % 2 == 0) {
                type = ValueType::TOMBSTONE;
            } else {
                type = ValueType::VALUE;
            }
            log_writer->write_record(key, value, type);
        }
    }
};

// Verifies default constructor attributes are assigned correctly.
// Only test wal_path and latest_segment.
TEST_F(LogReaderTest, DefaultConstructorInitializesCorrectly) {
    LogReader log_reader;
    EXPECT_EQ(log_reader.get_wal_path(), "./data/wal");
    EXPECT_EQ(log_reader.get_latest_segment_num(), 0);
}

// Verifies correct construction assuming records were written to LogWriter first
// Only test wal_path and latest_segment_num.
TEST_F(LogReaderTest, NonDefaultConstructorInitializesCorrectly) {

    // Four record per file -> 250 files, latest path is 000250.log.
    write_sample_records(1000, log_writer4);

    // Instantiate LogReader.
    LogReader log_reader4(wal_path4);
    EXPECT_EQ(log_reader4.get_wal_path(), wal_path4);
    EXPECT_EQ(log_reader4.get_latest_segment_num(), 250);
}

// Test one single record.

// Put only one record.
TEST_F(LogReaderTest, RestoresSinglePutRecord) {
    log_writer->write_record("key1", "daniel", ValueType::VALUE);

    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    EXPECT_EQ(mem_table->get("key1"), "daniel");
}

// Put and delete.
TEST_F(LogReaderTest, RestoresTombstoneRecord) {
    log_writer->write_record("key1", "daniel", ValueType::VALUE);
    log_writer->write_record("key1", "", ValueType::TOMBSTONE);

    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    EXPECT_EQ(mem_table->get("key1"), std::nullopt);
}

// Put delete and put again.
TEST_F(LogReaderTest, RestoresSinglePutDeletePutRecord) {
    log_writer->write_record("key1", "daniel", ValueType::VALUE);
    log_writer->write_record("key1", "", ValueType::TOMBSTONE);
    log_writer->write_record("key1", "daniel", ValueType::VALUE);

    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    EXPECT_EQ(mem_table->get("key1"), "daniel");
}

// Test multiple records

// Multiple puts.
TEST_F(LogReaderTest, RestoresMultiplePutRecords) {

    // Write records to LogWriter,
    for (int i = 1; i <= 999; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        log_writer4->write_record(key, value, type_value);
    }

    // Instantiate LogReader, latest segment is 000249.log.
    LogReader log_reader(wal_path4);
    log_reader.restore_mem_table(*mem_table);

    // Read values back from MemTable. Must restore them correctly.
    for (int i = 1; i <= 999; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        EXPECT_EQ(mem_table->get(key), value);
    }

}

// Multiple deletes.
TEST_F(LogReaderTest, RestoresMultipleTombstoneRecords) {

    // Write records to LogWriter,
    for (int i = 1; i <= 999; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        // Write and immediately delete.
        log_writer4->write_record(key, value, type_value);
        log_writer4->write_record(key, value, type_tombstone);
    }

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);
    log_reader.restore_mem_table(*mem_table);

    // Read values back from MemTable.
    for (int i = 1; i <= 999; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        // Must be empty because values were deleted after insertion.
        EXPECT_EQ(mem_table->get(key), std::nullopt);
    }

}
