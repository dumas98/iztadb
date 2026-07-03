//
// Created by Daniel Dumas on 11/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"
#include "log_utils.h"
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
    LogWriter* log_writer4;
    LogWriter* log_writer_32mb;

    // Before Each.
    void SetUp() override {
        // Build test directories.

        std::filesystem::create_directories("./test_data/wal_32mb");
        std::filesystem::create_directories("./test_data/wal1");
        std::filesystem::create_directories("./test_data/wal_half");
        std::filesystem::create_directories("./test_data/wal1_half");
        std::filesystem::create_directories("./test_data/wal2");
        std::filesystem::create_directories("./test_data/wal4");

        // Experiment with different wal file size thresholds.
        log_writer1 = new LogWriter("./test_data/wal1", sizeof(WALRecord));
        log_writer_half = new LogWriter("./test_data/wal_half", sizeof(WALRecord) / 2);
        log_writer1_half = new LogWriter("./test_data/wal1_half",
                                         static_cast<uintmax_t>(sizeof(WALRecord) * 1.5));
        log_writer2 = new LogWriter("./test_data/wal2", sizeof(WALRecord) * 2);
        log_writer4 = new LogWriter("./test_data/wal4", sizeof(WALRecord) * 4);
        log_writer_32mb = new LogWriter("./test_data/wal_32mb", 32 * 1024 * 1024);
    }

    // After Each.
    void TearDown() override {
        // Free memory.
        delete log_writer1;
        delete log_writer_half;
        delete log_writer1_half;
        delete log_writer2;
        delete log_writer4;
        delete log_writer_32mb;

        // Drop test directory.
        std::filesystem::remove_all("./test_data/");
        std::cout << "Test Ended." << std::endl;
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


// 1. Test constructors

// Verifies default constructor attributes are assigned correctly.
// Only test wal_path and file size.
TEST_F(LogWriterTest, DefaultConstructorInitializesCorrectly) {
    LogWriter default_log_writer;
    EXPECT_EQ(default_log_writer.get_wal_path(), "./data/wal");
    EXPECT_EQ(default_log_writer.get_max_wal_file_size(), 32 * 1024 * 1024);
}

// Test each attribute from non-default constructor.
TEST_F(LogWriterTest, NonDefaultConstructorInitializesCorrectly) {
    EXPECT_EQ(log_writer1->get_wal_path(), "./test_data/wal1");
    EXPECT_EQ(log_writer1->get_max_wal_file_size(), sizeof(WALRecord));
    EXPECT_EQ(log_writer1->get_next_sequence(), 1);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 0);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal1/000001.log");
}

// 2. Test Attributes Rotation

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

    // One record per file -> 2026 files, next path is 002027.log.
    write_sample_records(2026, log_writer1);
    EXPECT_EQ(count_files("./test_data/wal1"), 2026);
    EXPECT_EQ(log_writer1->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer1->get_latest_segment_num(), 2026);
    EXPECT_EQ(log_writer1->get_write_path(), "./test_data/wal1/002027.log");

    // Below record size -> same as one record per file.
    write_sample_records(2026, log_writer_half);
    EXPECT_EQ(count_files("./test_data/wal_half"), 2026);
    EXPECT_EQ(log_writer_half->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer_half->get_latest_segment_num(), 2026);
    EXPECT_EQ(log_writer_half->get_write_path(), "./test_data/wal_half/002027.log");

    // 1.5x record size -> two records per file since one record isn't enough for threshold.
    write_sample_records(2026, log_writer1_half);
    EXPECT_EQ(count_files("./test_data/wal1_half"), 1013);
    EXPECT_EQ(log_writer1_half->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer1_half->get_latest_segment_num(), 1013);
    EXPECT_EQ(log_writer1_half->get_write_path(), "./test_data/wal1_half/001014.log");

    // Two records per file -> 1013 files.
    write_sample_records(2026, log_writer2);
    EXPECT_EQ(count_files("./test_data/wal2"), 1013);
    EXPECT_EQ(log_writer2->get_next_sequence(), 2027);
    EXPECT_EQ(log_writer2->get_latest_segment_num(), 1013);
    EXPECT_EQ(log_writer2->get_write_path(), "./test_data/wal2/001014.log");
}

// Test actual 32MB threshold with 85K records.
TEST_F(LogWriterTest, RotationWith32MBThreshold) {

    // Insert enough records to trigger exactly one rotation.
    write_sample_records(85381, log_writer_32mb);
    EXPECT_EQ(count_files("./test_data/wal_32mb"), 1);
    EXPECT_EQ(log_writer_32mb->get_latest_segment_num(), 1);
    EXPECT_EQ(log_writer_32mb->get_write_path(), "./test_data/wal_32mb/000002.log");
    EXPECT_EQ(log_writer_32mb->get_next_sequence(), 85382);
}

// 3. Test Checksum

// Checksum is positive non-zero after write.
TEST_F(LogWriterTest, NonZeroChecksumAfterWrite) {

    write_sample_records(100, log_writer_32mb);

    // Open file.
    ifstream file(log_writer_32mb->get_write_path(), ios::binary);

    WALRecord wal_record;

    // Read all records sequentially.
    while(file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord))) {
        // Calculated checksum can't be zero.
        EXPECT_NE(wal_record.checksum, 0);
    }

    file.close();
}

// Two identical records have different checksum value.
TEST_F(LogWriterTest, SameRecordsProduceDifferentChecksum) {

    // Write two identical records.
    log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);
    log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);

    // Open file.
    ifstream file(log_writer_32mb->get_write_path(), ios::binary);

    // Read the checksum of each record.
    WALRecord wal_record;
    file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
    uint32_t first_record_crc32 = wal_record.checksum;

    file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
    uint32_t second_record_crc32 = wal_record.checksum;

    file.close();

    // Checksums must match.
    EXPECT_NE(first_record_crc32, second_record_crc32);
}

// Two identical records have different checksum value.
TEST_F(LogWriterTest, DifferentRecordsProduceDifferentChecksum) {

    // Write two identical records.
    log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);
    log_writer_32mb->write_record("username", "daniel", ValueType::TOMBSTONE);

    // Open file.
    ifstream file(log_writer_32mb->get_write_path(), ios::binary);

    // Read the checksum of each record.
    WALRecord wal_record;
    file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
    uint32_t first_record_crc32 = wal_record.checksum;

    file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
    uint32_t second_record_crc32 = wal_record.checksum;

    file.close();

    // Checksums must match.
    EXPECT_NE(first_record_crc32, second_record_crc32);
}

// Recalculate checksum after write and it must match.
TEST_F(LogWriterTest, RoundTripChecksumMatches) {

    // Write one record.
    log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);

    // Open file.
    ifstream file(log_writer_32mb->get_write_path(), ios::binary);

    // Read checksum.
    WALRecord wal_record;
    file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
    uint32_t original_checksum = wal_record.checksum;

    file.close();

    // Recalculate checksum.
    uint32_t recalculated_checksum = iztadb::wal::calculate_crc32(wal_record);

    EXPECT_EQ(recalculated_checksum, original_checksum);
}

// 4. File corruption, partial writes.

// Corrupt file keys.
TEST_F(LogWriterTest, FileCorruptionKeys) {

        // Write one record.
        log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);

        // Open file.
        fstream file(log_writer_32mb->get_write_path(), ios::binary | ios::in | ios::out);

        // Get calculated checksum.
        WALRecord wal_record;
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
        uint32_t original_checksum = wal_record.checksum;

        // Jump to the start of the key field.
        file.seekp(offsetof(WALRecord, key));

        // Insert in beginning of key AA -> "AA\0rname", remember char array decays to pointer of
        // first element.
        char corrupt[3] = "AA";
        file.write(corrupt,sizeof(corrupt));

        // Return to beginning of file.
        file.seekg(0);

        // Get record.
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));

        file.close();

        // Recalculate Checksum.
        uint32_t recalculated_checksum = iztadb::wal::calculate_crc32(wal_record);

        EXPECT_NE(recalculated_checksum, original_checksum);

        // New record is corrupted username.
        string corrupted_string("AA\0rname", 8);
        string wal_record_key(wal_record.key, 8);
        EXPECT_EQ(corrupted_string, wal_record_key);
    }

// Corrupt record type.
TEST_F(LogWriterTest, FileCorruptionType) {
        // Write one record.
        log_writer_32mb->write_record("username", "daniel", ValueType::VALUE);

        // Open file.
        fstream file(log_writer_32mb->get_write_path(), ios::binary | ios::in | ios::out);

        // Get original checksum.
        WALRecord wal_record;
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
        uint32_t original_checksum = wal_record.checksum;

        // Jump to the start of the type field.
        file.seekp(offsetof(WALRecord, type));

        // Write a corrupt record.
        uint8_t corrupt_wal_record_type = static_cast<uint8_t>(ValueType::TOMBSTONE);
        file.write(reinterpret_cast<char*>(&corrupt_wal_record_type), sizeof(corrupt_wal_record_type));

        // Return to beginning of file and calculate checksum again.
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
        uint32_t corrupt_checksum = iztadb::wal::calculate_crc32(wal_record);

        // Numbers must differ.
        EXPECT_NE(original_checksum, corrupt_checksum);

        // Jump to the start of the type field.
        file.seekp(offsetof(WALRecord, type));

        // Fix the file to its original state.
        uint8_t fix_wal_record_type = static_cast<uint8_t>(ValueType::VALUE);
        file.write(reinterpret_cast<char*>(&fix_wal_record_type), sizeof(fix_wal_record_type));

        // Return to beginning of file and calculate checksum again.
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
        file.close();
        uint32_t fixed_checksum = iztadb::wal::calculate_crc32(wal_record);

        // Numbers must match.
        EXPECT_EQ(original_checksum, fixed_checksum);

    }

// Partial Write, second record gets truncated.
TEST_F(LogWriterTest, TruncatedRecordDetected){
        // Write two records.
        log_writer_32mb->write_record("username1", "daniel", ValueType::VALUE);
        log_writer_32mb->write_record("username2", "paul", ValueType::VALUE);

        string wal_path = log_writer_32mb->get_write_path();
        // Shrink file to cut second record in half.
        uintmax_t full_size = std::filesystem::file_size(wal_path);
        uintmax_t truncated_size = full_size - (sizeof(WALRecord) / 2);

        std::filesystem::resize_file(wal_path, truncated_size);

        // Try to read both records.
        std::ifstream file(wal_path, std::ios::binary);
        WALRecord wal_record;
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));

        // First record has same size of WALRecord, key-value pairs match.
        EXPECT_EQ(file.gcount(), sizeof(WALRecord));
        EXPECT_EQ(string(wal_record.key), "username1");
        EXPECT_EQ(string(wal_record.value), "daniel");

        // Second record is smaller than wall record.
        file.read(reinterpret_cast<char*>(&wal_record),sizeof(WALRecord));
        EXPECT_NE(file.gcount(), sizeof(WALRecord));
    }

// 5. Test reading all records back across segments matches what was written.
TEST_F(LogWriterTest, ReadAllRecordsMatchesWrites) {
        // Write 15 records, 4 total segments on a LogWriter that allows 4 records per file.
        write_sample_records(15, log_writer4);

        // Iterate through each segment and verify records match.
        int max_segment = log_writer4->get_latest_segment_num();
        int expected_num = 1;

        for (int i = 1; i <= max_segment; i++) {

            // Open current path.
            string path = "./test_data/wal4/" + format("{:06d}.log", i);
            ifstream file(path, ios::binary);

            // Read each record of the segment until it reaches the end.
            WALRecord wal_record;
            while (file.read(reinterpret_cast<char*>(&wal_record), sizeof(WALRecord))) {

                // CLOSE record marks clean end of segment, skip to next segment.
                if (static_cast<ValueType>(wal_record.type) == ValueType::CLOSE) {
                    break;
                }

                // Calculate expected key, value and type.
                string expected_key = "key" + std::to_string(expected_num);
                string expected_value = "value" + std::to_string(expected_num);
                ValueType expected_type;

                // Odd numbers get VALUE, even numbers get TOMBSTONE.
                if (expected_num % 2 == 0) {
                    expected_type = ValueType::TOMBSTONE;
                } else {
                    expected_type = ValueType::VALUE;
                }

                // Attributes must match.
                EXPECT_EQ(wal_record.sequence_number, expected_num);
                EXPECT_EQ(string(wal_record.key), expected_key);
                EXPECT_EQ(string(wal_record.value), expected_value);
                EXPECT_EQ(static_cast<ValueType>(wal_record.type), expected_type);

                // Verify checksum integrity.
                EXPECT_EQ(wal_record.checksum, iztadb::wal::calculate_crc32(wal_record));

                expected_num++;
            }
        }
    }

// 6. Test that each rotated segment ends with a CLOSE record.
// Last segment has space to insert more records.
TEST_F(LogWriterTest, EachRotatedSegmentEndsWithCloseRecord) {
        // Write 15 records, 4 per segment. Segment 4 stays active.
        write_sample_records(15, log_writer4);

        int max_segment = log_writer4->get_latest_segment_num();

        // Read each segment.
        for (int i = 1; i <= max_segment; i++) {
            string path = "./test_data/wal4/" + format("{:06d}.log", i);
            ifstream file(path, ios::binary);

            // Read through the entire segment to land on the last record.
            WALRecord wal_record;
            WALRecord last_record;

            while (file.read(reinterpret_cast<char*>(&wal_record), sizeof(WALRecord))) {
                last_record = wal_record;
            }

            if (i < max_segment) {
                // Rotated segment, must end with CLOSE.
                EXPECT_EQ(static_cast<ValueType>(last_record.type), ValueType::CLOSE)
                    << "Segment " << i << " did not end with a CLOSE record.";
            } else {
                // Active segment, should not have a CLOSE record.
                EXPECT_NE(static_cast<ValueType>(last_record.type), ValueType::CLOSE)
                    << "Active segment " << i << " should not be closed yet.";
            }
        }
    }

// Last segment is out of space, must contain a CLOSE Type.
TEST_F(LogWriterTest, EachRotatedSegmentEndsWithCloseRecordWriteBoundary) {
        // Write 16 records, 4 per segment. Segment 4 is full.
        write_sample_records(16, log_writer4);

        int max_segment = log_writer4->get_latest_segment_num();

        // Read each segment.
        for (int i = 1; i <= max_segment; i++) {
            string path = "./test_data/wal4/" + format("{:06d}.log", i);
            ifstream file(path, ios::binary);

            // Read through the entire segment to land on the last record.
            WALRecord wal_record;
            WALRecord last_record;

            while (file.read(reinterpret_cast<char*>(&wal_record), sizeof(WALRecord))) {
                last_record = wal_record;
            }

            // All latest records contain a CLOSE value type.
            EXPECT_EQ(static_cast<ValueType>(last_record.type), ValueType::CLOSE);
        }
    }

// Last segment is out of space, must contain a CLOSE Type.
TEST_F(LogWriterTest, EachRotatedSegmentEndsWithCloseRecordWriteBoundaryFirst) {
        // Write 16 records, 4 per segment. Segment 4 is full.
        write_sample_records(4, log_writer4);

        string path = "./test_data/wal4/000001.log";
        ifstream file(path, ios::binary);

        // Read through the entire segment to land on the last record.
        WALRecord wal_record;
        WALRecord last_record;

        while (file.read(reinterpret_cast<char*>(&wal_record), sizeof(WALRecord))) {
            last_record = wal_record;
        }

        // All latest records contain a CLOSE value type.
        EXPECT_EQ(static_cast<ValueType>(last_record.type), ValueType::CLOSE);


        }