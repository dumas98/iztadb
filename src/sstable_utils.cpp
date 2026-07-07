//
// Created by Daniel Dumas on 03/07/26.
//

#pragma once
#include "sstable_utils.h"
#include <filesystem>
#include <zlib.h>

namespace iztadb::sstable {
    int calculate_latest_segment(const std::string& sst_path) {
        int latest_segment_num = 0;

        // No files exist, use zero.
        if (std::filesystem::is_empty(sst_path)) {
            return latest_segment_num;
        }

        // Iterate through each directory to get max (corresponds to latest file).
        for (auto const& dir_entry : std::filesystem::directory_iterator(sst_path)) {
            // Filter out non .log files.
            if (dir_entry.path().extension() != ".sst") continue;

            // Get only file name without extension with stem() and convert to integer.
            int num = std::stoi(dir_entry.path().stem().string());

            if (num > latest_segment_num) {
                latest_segment_num = num;
            }
        }

        return latest_segment_num;
    }

    uint32_t calculate_crc32(const std::vector<uint8_t>& block_buffer) {
        return crc32(0, reinterpret_cast<const Bytef*>(block_buffer.data()), block_buffer.size());
    }
}
