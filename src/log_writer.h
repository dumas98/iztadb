//
// Created by Daniel Dumas on 05/06/26.
//


#include <string>
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include "types.h"

const uintmax_t MAX_WAL_FILE_SIZE = 100; // Small size to test.

/**
 * @brief Represents a single record in the Write-Ahead Log.
 *
 * Fixed size layout of 393 bytes per record. Padding is disabled
 * via pragma pack to ensure exact byte layout on disk.
 * Sequence_number is an increasing sequence to identify record,
 * CRC32 checksum is used to detect corruption on recovery, type
 * indicates it's value of ValueType.
 *
 * Keys will only support strings with a maximum of 128 chars
 * and values with a maximum of 256 chars.
 */
#pragma pack(1)
struct WALRecord {
    uint32_t sequence_number;  // 4 bytes
    uint32_t checksum;         // 4 bytes
    uint8_t type;              // 1 byte
    char key[128];             // 128 bytes
    char value[256];           // 256 bytes
};
#pragma pack()

/**
 * @brief Manages write operations to the Write-Ahead Log (WAL).
 *
 * On construction, resolves the active log file by scanning the WAL
 * directory for the latest file (contains the highest number).
 * Appends only one record to such file.
 *
 * When the active file exceeds MAX_WAL_FILE_SIZE a new
 * file is created automatically.
 *
 * Log files are named sequentially with 5 leading zeros:
 *   000001.log, 000002.log, 000003.log ...
 */
class LogWriter {
    private:
        std::string wal_path;
        uintmax_t max_wal_file_size;
        std::string write_path;
        uint32_t next_sequence;
        int latest_segment_num;

    public:
        /**
         * @brief Constructs the LogWriter and initializes WAL state.
         *
         * Resolves the active log segment, reads the latest sequence number
         * from disk and increments it. If no segments exist initializes
         * sequence to 1 and creates 000001.log on first write.
         *
         * @param wal_path Path to the WAL directory. Defaults to "./data/wal".
         * @param max_size Maximum segment size in bytes before rotation. Defaults to MAX_WAL_FILE_SIZE.
         */
        explicit LogWriter(
                        const std::string& wal_path = "./data/wal",
                        uintmax_t max_size = MAX_WAL_FILE_SIZE
                        );


        /**
         * @brief Returns the highest numbered log segment in the WAL directory.
         *
         * @return Max segment number, 0 if directory is empty.
         */
        int calculate_latest_segment();

        /**
         * @brief Resolves the path of the active log file.
         * Uses get_max_segment to calculate the maximum segment.
         *
         * If active log file exceeds MAX_WAL_FILE_SIZE the new
         * path will be active log file + 1:
         *  ./data/wal/000008.log -> active log file size exceeds threshold
         *  ./data/wal/000009.log -> value returned
         *
         * @return Full path to the active log file.
         */
        std::string resolve_active_log_path();

        /**
         * @brief Retrieves the next available sequence number.
         *
         * If no WAL segments exist, returns 1. Otherwise, opens the latest
         * segment, goes to the last record, and returns its sequence
         * number plus one.
         *
         * @return Next sequence number to use for the upcoming write.
         * @throws std::runtime_error if the latest WAL segment cannot be opened.
         */
        uint32_t calculate_next_sequence();

        /**
         * @brief Calculates CRC32 checksum for a WAL record.
         *
         * Sets to zero the checksum field before calculating to ensure
         * the checksum is not included in its own calculation.
         * Used to verify record integrity on recovery.
         *
         * @param record WALRecord to calculate checksum, passed by value
         * so the original is not modified.
         * @return CRC32 checksum of the record.
         */
        static uint32_t calculate_crc32(WALRecord record);

        /**
         * @brief Writes a single record to the active WAL segment.
         *
         * Builds a WALRecord from the provided key, value type and sequence attribute
         * Calculates its CRC32 checksum and appends it to the active log segment.
         * Increments the sequence number after each write and recalculates
         * the active log path to handle segment rotation.
         *
         * @param key The record key, limited to 128 bytes.
         * @param value The record value, limited to 256 bytes.
         * @param type The record type, either VALUE or TOMBSTONE.
         */
        void write_record(const std::string& key, const std::string& value, ValueType type);

        /**
         * @brief Returns the WAL directory path.
         * @return Path to the WAL directory.
         */
        std::string get_wal_path();

        /**
         * @brief Returns the maximum WAL file size before rotation.
         * @return Maximum file size in bytes.
         */
        uintmax_t get_max_wal_file_size();

        /**
         * @brief Returns the path of the active log file for writing.
         * @return Full path to the active log file.
         */
        std::string get_write_path();

        /**
         * @brief Returns the next sequence number to be used for writing.
         * @return Next sequence number.
         */
        uint32_t get_next_sequence();

        /**
         * @brief Returns the highest numbered log segment.
         * @return Max segment number, 0 if no segments exist.
         */
        int get_max_file();
};



