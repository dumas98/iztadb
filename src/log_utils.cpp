//
// Created by Daniel Dumas on 22/06/26.
//

#include "log_utils.h"
#include <filesystem>
#include <zlib.h>

namespace iztadb::wal {

    int calculate_latest_segment(const std::string& wal_path) {
        int latest_segment_num = 0;

        // A directory that was never created holds no segments. Checked before
        // is_empty(), which throws instead of returning true on a missing path.
        if (!std::filesystem::exists(wal_path)) {
            return latest_segment_num;
        }

        // No files exist, use zero.
        if (std::filesystem::is_empty(wal_path)) {
            return latest_segment_num;
        }

        // Iterate through each directory to get max (corresponds to latest file).
        for (auto const& dir_entry : std::filesystem::directory_iterator(wal_path)) {
            // Filter out non .log files.
            if (dir_entry.path().extension() != ".log") continue;

            // Get only file name without extension with stem() and convert to integer.
            int num = std::stoi(dir_entry.path().stem().string());

            if (num > latest_segment_num) {
                latest_segment_num = num;
            }
        }

        return latest_segment_num;
    }

    uint32_t calculate_crc32(WALRecord record) {
        // Set to zero so contents match when reading.
        record.checksum = 0;

        // Start from 0, insert the address of record copy and scan the size of WALRecord.
        return crc32(0, reinterpret_cast<const Bytef*>(&record), sizeof(WALRecord));
    };

}