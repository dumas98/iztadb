//
// Created by Daniel Dumas on 27/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"
#include "log_reader.h"
#include "memtable.h"
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
    std::string wal_path1_half = "./test_data/wal1_half";

    ValueType type_value = ValueType::VALUE;
    ValueType type_tombstone = ValueType::TOMBSTONE;

    // Use pointer to allocate memory preventing the default constructor attribute assignment.
    LogWriter* log_writer;
    LogWriter* log_writer4;
    LogWriter* log_writer1_half;
    MemTable* mem_table;

    // Before Each.
    void SetUp() override {
        // Build test directories.

        std::filesystem::create_directories(wal_path);
        std::filesystem::create_directories(wal_path4);
        std::filesystem::create_directories(wal_path1_half);

        // Instantiate LogWriter, LogReader and MemTable
        log_writer = new LogWriter(wal_path);
        log_writer1_half = new LogWriter("./test_data/wal1_half",
                                         static_cast<uintmax_t>(sizeof(WALRecord) * 1.5));
        log_writer4 = new LogWriter(wal_path4, sizeof(WALRecord) * 4);
        mem_table = new MemTable();
    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete log_writer;
        delete log_writer4;
        delete mem_table;
        delete log_writer1_half;

        // Drop test directory.
        std::filesystem::remove_all(wal_path);
        std::filesystem::remove_all(wal_path4);
        std::filesystem::remove_all(wal_path1_half);

        std::cout << "Test Ended." << std::endl;
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

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)));
}

// Put and delete.
TEST_F(LogReaderTest, RestoresTombstoneRecord) {
    log_writer->write_record("key1", "daniel", ValueType::VALUE);
    log_writer->write_record("key1", "", ValueType::TOMBSTONE);

    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    EXPECT_EQ(mem_table->get("key1"), std::nullopt);

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*2));
}

// Put delete and put again.
TEST_F(LogReaderTest, RestoresSinglePutDeletePutRecord) {
    log_writer->write_record("key1", "daniel", ValueType::VALUE);
    log_writer->write_record("key1", "", ValueType::TOMBSTONE);
    log_writer->write_record("key1", "daniel", ValueType::VALUE);

    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    EXPECT_EQ(mem_table->get("key1"), "daniel");

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*3));
}

// Test PUT multiple records and segment rotation.

// Multiple puts.

// Segment of 4 WAL Records.
TEST_F(LogReaderTest, RestoresMultipleSegmentsFourRecordsEach) {
    // Multiple writes.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Restore using memTable.
    LogReader log_reader(wal_path4);
    log_reader.restore_mem_table(*mem_table);

    for (int i = 1; i <= 999; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000250.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*3));
}

// Segment of 2 WAL Records, max size of 1 and a half, doesn't read EOF.
TEST_F(LogReaderTest, RestoresMultipleSegmentsOneAndHalfRecordsEachNoEOF) {
    // Multiple writes.
    for (int i = 1; i <= 9; i++) {
        log_writer1_half->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Restore using memTable.
    LogReader log_reader(wal_path1_half);
    log_reader.restore_mem_table(*mem_table);

    for (int i = 1; i <= 9; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path1_half + "/000005.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)));
}

// Multiple Segments One and a Half.
TEST_F(LogReaderTest, RestoresMultipleSegmentsOneAndHalfRecordsEachEOFReached) {
    // Multiple writes.
    for (int i = 1; i <= 10; i++) {
        log_writer1_half->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Restore using memTable.
    LogReader log_reader(wal_path1_half);
    log_reader.restore_mem_table(*mem_table);

    for (int i = 1; i <= 10; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, true);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path1_half + "/000005.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, sizeof(WALRecord)*3);
}

// Segment of 32 MB.
TEST_F(LogReaderTest, RestoresMultipleSegments32MB) {
    // Multiple writes, fill at least one segment.
    for (int i = 1; i <= 86000; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    LogReader log_reader(wal_path);

    // Check correct Rotation.
    EXPECT_EQ(log_reader.get_latest_segment_num(), 2);

    // Recreate MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Check recreated MemTable.
    for (int i = 1; i <= 86000; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000002.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*619));
}

// Multiple deletes.

// Segment of 4 WAL Records.
TEST_F(LogReaderTest, RestoresMultipleTombstoneRecordsSegmentsFourRecordsEach) {

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

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000500.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*2));

}

// Segment of 32MB.
TEST_F(LogReaderTest, RestoresMultipleTombstoneRecordsSegments32MB) {

    // Write records to LogWriter,
    for (int i = 1; i <= 86000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        // Write and immediately delete.
        log_writer->write_record(key, value, type_value);
        log_writer->write_record(key, value, type_tombstone);
    }

    // Instantiate LogReader.
    LogReader log_reader(wal_path);

    // Check correct Rotation.
    EXPECT_EQ(log_reader.get_latest_segment_num(), 3);
    log_reader.restore_mem_table(*mem_table);

    log_reader.restore_mem_table(*mem_table);

    // Read values back from MemTable.
    for (int i = 1; i <= 86000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        // Must be empty because values were deleted after insertion.
        EXPECT_EQ(mem_table->get(key), std::nullopt);
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000003.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*1238));

}

// Multiple put delete put, not consecutive.

// Segment of 4 WAL Records.
TEST_F(LogReaderTest, RestoresMultiplePutsDeletePutRecordsSegmentsFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Repeat for double PUT.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);
    log_reader.restore_mem_table(*mem_table);

    // Check recreated MemTable.
    for (int i = 1; i <= 999; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Delete Records.
    for (int i = 1; i <= 999; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        // Write, delete and write again.
        log_writer4->write_record(key, value, type_tombstone);
    }

    // Re-instantiate MemTable.
    delete mem_table;
    mem_table = new MemTable();
    LogReader log_reader2(wal_path4);
    log_reader2.restore_mem_table(*mem_table);

    // Check recreated MemTable, no records appear.
    for (int i = 1; i <= 999; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
    }

    // Write records back to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Re-instantiate MemTable.
    delete mem_table;
    mem_table = new MemTable();
    LogReader log_reader3(wal_path4);
    log_reader3.restore_mem_table(*mem_table);
    log_reader3.restore_mem_table(*mem_table);

    // Check recreated MemTable.
    for (int i = 1; i <= 999; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader3.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader3.get_recovery_result().reached_eof, true);
    EXPECT_EQ(log_reader3.get_recovery_result().segment, wal_path4 + "/000999.log");
    EXPECT_EQ(log_reader3.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*5));

}

// Segment of 32MB.
TEST_F(LogReaderTest, RestoresMultiplePutDeletePutRecordsSegments32MB) {

    // Write records to LogWriter.
    for (int i = 1; i <= 25000; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Repeat for double PUT.
    for (int i = 1; i <= 25000; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Instantiate LogReader.
    LogReader log_reader(wal_path);
    log_reader.restore_mem_table(*mem_table);

    // Check recreated MemTable.
    for (int i = 1; i <= 25000; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Delete Records.
    for (int i = 1; i <= 25000; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        // Write, delete and write again.
        log_writer->write_record(key, value, type_tombstone);
    }

    // Re-instantiate MemTable.
    delete mem_table;
    mem_table = new MemTable();
    LogReader log_reader2(wal_path);
    log_reader2.restore_mem_table(*mem_table);

    // Check recreated MemTable, no records appear.
    for (int i = 1; i <= 25000; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
    }

    // Write records back to LogWriter.
    for (int i = 1; i <= 25000; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Re-instantiate MemTable.
    delete mem_table;
    mem_table = new MemTable();
    LogReader log_reader3(wal_path);
    log_reader3.restore_mem_table(*mem_table);
    log_reader3.restore_mem_table(*mem_table);

    // Check correct Rotation.
    EXPECT_EQ(log_reader3.get_latest_segment_num(), 2);

    // Check recreated MemTable.
    for (int i = 1; i <= 25000; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader3.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader3.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader3.get_recovery_result().segment, wal_path + "/000002.log");
    EXPECT_EQ(log_reader3.get_recovery_result().last_clean_offset, (sizeof(WALRecord)*14619));

}

// Corrupt the data.

// Corrupt Value, segment of 4 records.
TEST_F(LogReaderTest, RestoresCorrputedValueDataSegmentsFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file where value500 exists.
    std::fstream file(wal_path4 + "/000125.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 500 value address.
    WALRecord wal_record;
    file.seekp((sizeof(WALRecord)* 3) + offsetof(WALRecord, value));

    // Insert in beginning of value DDD -> "DDD\0e500", remember char array decays to pointer of
    // first element.
    char corrupt[4] = "DDD";
    file.write(corrupt,sizeof(corrupt));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Up to 499 records were uncorrupt, LogReader completely restores it.
    // from 500 onwards no records exist in MemTable.
    for (int i = 1; i <= 999; i++) {
        if (i <= 499) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }
    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000125.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 3));

}

// Corrupt Value, segment of 32MB.
TEST_F(LogReaderTest, RestoresCorrputedValueDataSegments32MB) {

    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file.
    std::fstream file(wal_path + "/000001.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 500 value address.
    WALRecord wal_record;
    file.seekp((sizeof(WALRecord)* 499) + offsetof(WALRecord, value));

    // Insert in beginning of value DDD -> "DDD\0e500", remember char array decays to pointer of
    // first element.
    char corrupt[4] = "DDD";
    file.write(corrupt,sizeof(corrupt));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Up to 499 records were uncorrupt, LogReader completely restores it.
    // from 500 onwards no records exist in MemTable.
    for (int i = 1; i <= 999; i++) {
        if (i <= 499) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 499));
}

// Corrupt ValueType, segment of 4 records.
TEST_F(LogReaderTest, RestoresCorrputedValueTypeDataSegmentsFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file where value500 exists.
    std::fstream file(wal_path4 + "/000125.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 500 value address.
    WALRecord wal_record;
    file.seekp((sizeof(WALRecord)* 3) + offsetof(WALRecord, type));

    // Write a corrupt record, change to TOMBSTONE.
    uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
    file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Up to 499 records were uncorrupt, LogReader completely restores it.
    // from 500 onwards no records exist in MemTable.
    for (int i = 1; i <= 999; i++) {
        if (i <= 499) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000125.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 3));
}

// Corrupt ValueType, segment of 32MB.
TEST_F(LogReaderTest, RestoresCorrputedValueTypeDataSegments32MB) {

    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file.
    std::fstream file(log_writer->get_write_path(), std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 500 value address.
    WALRecord wal_record;
    file.seekp((sizeof(WALRecord)* 499) + offsetof(WALRecord, value));

    // Write a corrupt record, change to TOMBSTONE.
    uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
    file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Up to 499 records were uncorrupt, LogReader completely restores it.
    // from 500 onwards no records exist in MemTable.
    for (int i = 1; i <= 999; i++) {
        if (i <= 499) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, log_writer->get_write_path());
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 499));
}

// Corrupt Close record.
// Corrupt CloseType, segment of 4 records, first Segment corrupted.
TEST_F(LogReaderTest, RestoresCorrputedCloseTypeDataFirstSegmentFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file.
    std::fstream file(wal_path4 + "/000001.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 4 type address.
    file.seekp((sizeof(WALRecord)* 4) + offsetof(WALRecord, type));

    // Write a corrupt record, change to VALUE.
    uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
    file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Corrupt CloseType, segment of 4 records, between Segment corrupted.
TEST_F(LogReaderTest, RestoresCorrputedCloseTypeDataBetweenSegmentsFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 11; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file.
    std::fstream file(wal_path4 + "/000002.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 4 value address.
    file.seekp((sizeof(WALRecord)* 4) + offsetof(WALRecord, type));

    // Write a corrupt record, change to VALUE.
    uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
    file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000002.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Corrupt CloseType, segment of 4 records, final segment.
TEST_F(LogReaderTest, RestoresCorrputedCloseTypeDataFinalSegmentFourRecordsEach) {

    // Write records to LogWriter.
    for (int i = 1; i <= 12; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Open file.
    std::fstream file(wal_path4 + "/000003.log", std::ios::binary | std::ios::in | std::ios::out);

    // Corrupt file.

    // Jump to record 4 value address.
    WALRecord wal_record;
    file.seekp((sizeof(WALRecord)* 4) + offsetof(WALRecord, type));

    // Write a corrupt record, change to TOMBSTONE, (warning, corrupting with VALUE is not corruption because
    // VALUE = 0).
    uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
    file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

    file.close();

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000003.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Partial writes inside a record, truncate the file.
// First Segment.
// Segment of 4 records, partial write inside record.
TEST_F(LogReaderTest, RestoresPartialWriteInsideRecordFSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate fourth record in half.
    std::string partial_file_path = wal_path4 + "/000001.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 3.5);
    std::filesystem::resize_file(partial_file_path, truncated_size);

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 3.
    for (int i = 1; i <= 4; i++) {
        if (i <= 3) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 3));
}

// Partial write between records, truncate the file.
// Segment of 4 records, partial write between records.
TEST_F(LogReaderTest, RestoresPartialWriteBetweenRecordFSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate third record.
    std::string partial_file_path = wal_path4 + "/000001.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 3);
    std::filesystem::resize_file(partial_file_path, truncated_size);


    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 3.
    for (int i = 1; i <= 4; i++) {
        if (i <= 3) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    // CAUTION: If record is truncated between the next record, (non-close) there's no way to know and
    // will return its clean.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 3));
}

// Test Recovery Result is Working Correctly
// Partial writes inside a record, truncate the file.
// Segment of 4 records, partial write inside CLOSE record.
TEST_F(LogReaderTest, PartialWriteCloseRBetweenRecordFSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate CLOSE record.
    std::string partial_file_path = wal_path4 + "/000001.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 4) + (sizeof(WALRecord) / 2);
    std::filesystem::resize_file(partial_file_path, truncated_size);


    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 4
    for (int i = 1; i <= 4; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Partial write between records, truncate the file.
// Segment of 4 records, partial write removes CLOSE Record.
TEST_F(LogReaderTest, PartialWriteCloseRInsideRecordFSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate third record.
    std::string partial_file_path = wal_path4 + "/000001.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 4);
    std::filesystem::resize_file(partial_file_path, truncated_size);


    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 630 (first truncated record),
    // even though there are two truncated files.
    for (int i = 1; i <= 4; i++) {
        if (i <= 4) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Truncate first segment completely.
TEST_F(LogReaderTest, TruncateSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 4; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Complete segment.
    std::string partial_file_path = wal_path4 + "/000001.log";
    uintmax_t truncated_size = 0;
    std::filesystem::resize_file(partial_file_path, truncated_size);


    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // No record exist, file was truncated.
    for (int i = 1; i <= 4; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
    }

    // Check correct recovery_result.
    // WARNING, returns clean even if data existed before. Works correctly.
    EXPECT_EQ(log_reader.get_recovery_result().clean, true);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000001.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, 0);
}

// Between Segments.
// Segment of 4 records, partial write inside record.
TEST_F(LogReaderTest, RestoresPartialWritesInsideRecordBetweenSegmentsFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate two files.

    // Record 631: CEIL(631/4) = 000158.log, 3rd record.
    // Keep 2 full records (629, 630) + half of the 3rd (631) = partial write.
    std::string partial_file_path = wal_path4 + "/000158.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 2) + (sizeof(WALRecord) / 2);
    std::filesystem::resize_file(partial_file_path, truncated_size);

    // Record 731: CEIL(731/4) = 000183.log, 3rd record.
    // This segment is unreachable once segment 158 fails,
    // included to confirm the reader stops at the first corruption, not the last.
    std::string partial_file_path2 = wal_path4 + "/000183.log";
    uintmax_t truncated_size2 = (sizeof(WALRecord) * 2) + (sizeof(WALRecord) / 2);
    std::filesystem::resize_file(partial_file_path2, truncated_size2);

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 630 (first truncated record),
    // even though there are two truncated files.
    for (int i = 1; i <= 999; i++) {
        if (i <= 630) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000158.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 2));

}

// Partial write between records, truncate the file.
// Segment of 4 records, partial write between records.
TEST_F(LogReaderTest, RestoresPartialWritesBetweenRecordSegmentsFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate two files.

    // Truncate exactly between Record 631 and Record 632.
    // CEIL(631/4) = 000158.log, third record.

    // Record 631: CEIL(631/4) = 000158.log, 3rd record.
    // Keep 3 full records (629, 630, 631)
    std::string partial_file_path = wal_path4 + "/000158.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 3);
    std::filesystem::resize_file(partial_file_path, truncated_size);

    // Truncate exactly between Record 731 and Record 732.
    // CEIL(731/4) = 000183.log, third record.
    // Keep 3 full records (729, 730, 731)
    std::string partial_file_path2 = wal_path4 + "/000183.log";
    uintmax_t truncated_size2 = (sizeof(WALRecord) * 3);
    std::filesystem::resize_file(partial_file_path2, truncated_size2);

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 631 (record is intact but file is truncated)
    // even though there are two truncated files.
    for (int i = 1; i <= 999; i++) {
        if (i <= 631) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000158.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 3));
}

// Test Recovery Result is Working Correctly
// Partial writes inside a record, truncate the file.
// Segment of 4 records, partial write inside CLOSE record.
TEST_F(LogReaderTest, PartialWriteCloseRBetweenRecordBetweenSegmentsFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate two files.

    // Truncate partially segment 158 CLOSE record.
    // Keep 4 full records (629, 630, 631, 632)
    std::string partial_file_path = wal_path4 + "/000158.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 4.5);
    std::filesystem::resize_file(partial_file_path, truncated_size);

    // Truncate partially segment 183 CLOSE record.
    // Keep 4 full records (729, 730, 731, 732)
    std::string partial_file_path2 = wal_path4 + "/000183.log";
    uintmax_t truncated_size2 = (sizeof(WALRecord) * 4.5);
    std::filesystem::resize_file(partial_file_path2, truncated_size2);

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 631 (record is intact but file is truncated)
    // even though there are two truncated files.
    for (int i = 1; i <= 999; i++) {
        if (i <= 632) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }


    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000158.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Partial write between records, truncate the file.
// Segment of 4 records, partial write removes CLOSE Record.
TEST_F(LogReaderTest, PartialWritesCloseRInsideRecordBetweenSegmentsFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 999; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate two files.

    // Truncate completely segment 158 CLOSE record.
    // Keep 4 full records (629, 630, 631, 632)
    std::string partial_file_path = wal_path4 + "/000158.log";
    uintmax_t truncated_size = (sizeof(WALRecord) * 4);
    std::filesystem::resize_file(partial_file_path, truncated_size);

    // Truncate completely segment 183 CLOSE record.
    // Keep 4 full records (729, 730, 731, 732)
    std::string partial_file_path2 = wal_path4 + "/000183.log";
    uintmax_t truncated_size2 = (sizeof(WALRecord) * 4);
    std::filesystem::resize_file(partial_file_path2, truncated_size2);

    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 631 (record is intact but file is truncated)
    // even though there are two truncated files.
    for (int i = 1; i <= 999; i++) {
        if (i <= 632) {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
        }
        else {
            EXPECT_EQ(mem_table->get("key" + std::to_string(i)), std::nullopt);
        }
    }


    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000158.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, (sizeof(WALRecord)* 4));
}

// Truncate between segment completely.
TEST_F(LogReaderTest, TruncateBetSegmentFourRecordsEach) {
    // Write records to LogWriter.
    for (int i = 1; i <= 10; i++) {
        log_writer4->write_record("key" + std::to_string(i), "value" + std::to_string(i), type_value);
    }

    // Truncate third record.
    std::string partial_file_path = wal_path4 + "/000002.log";
    uintmax_t truncated_size = 0;
    std::filesystem::resize_file(partial_file_path, truncated_size);


    // Instantiate LogReader.
    LogReader log_reader(wal_path4);

    // Restore MemTable.
    log_reader.restore_mem_table(*mem_table);

    // MemTable is restored up to record 4, because of truncated segment.
    for (int i = 1; i <= 4; i++) {
        EXPECT_EQ(mem_table->get("key" + std::to_string(i)), "value" + std::to_string(i));
    }

    // Check correct recovery_result.
    EXPECT_EQ(log_reader.get_recovery_result().clean, false);
    EXPECT_EQ(log_reader.get_recovery_result().reached_eof, false);
    EXPECT_EQ(log_reader.get_recovery_result().segment, wal_path4 + "/000002.log");
    EXPECT_EQ(log_reader.get_recovery_result().last_clean_offset, 0);
}
