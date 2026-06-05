//
// Created by Daniel Dumas on 05/06/26.
//

#include <cstdint>
#include <string>
#include <filesystem>

const uintmax_t MAX_WAL_FILE_SIZE = 10; // Small size to test.

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

    public:
        /**
         * @brief Constructs the LogWriter.
         * Calls resolve_log_path to open the active log file.
         */
        LogWriter();
        /**
         * @brief Resolves the path of the active log file.
         * Scans the WAL directory for the highest numbered file and
         * returns the path to it,
         *
         * If active log file exceeds MAX_WAL_FILE_SIZE the new
         * path will be active log file + 1:
         *  ./data/wal/000008.log -> active log file size exceeds threshold
         *  ./data/wal/000009.log -> value returned
         *
         * @return Full path to the active log file.
         */
        static std::string resolve_log_path();

        /**
         * @brief Returns the path of the active log file for writing.
         *
         * @return Full path to the active log file.
         */
        std::string get_write_path();
};

struct WALFile {
    uint32_t sequence_number; // 4 Bytes.
};


