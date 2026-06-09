//
// Created by Daniel Dumas on 05/06/26.
//


#include <string>
#include <filesystem>
#include <fstream>

const uintmax_t MAX_WAL_FILE_SIZE = 100; // Small size to test.

/**
 * @brief Manages write operations to the Write-Ahead Log (WAL).
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
        std::string write_path;
        uint32_t sequence_number;

    public:
        /**
         * @brief Constructs the LogWriter.
         * Calls resolve_log_path to open the active log file.
         */
        LogWriter();

        /**
         * @brief Returns the highest numbered log segment in the WAL directory.
         * @return Max segment number, 0 if directory is empty.
         */
        static int get_max_segment();

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
        static std::string resolve_active_log_path(uintmax_t max_size = MAX_WAL_FILE_SIZE) ;

        /**
         * @brief Returns the path of the active log file for writing.
         *
         * @return Full path to the active log file.
         */
        std::string get_write_path();

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
        static uint32_t get_latest_sequence();
};

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

