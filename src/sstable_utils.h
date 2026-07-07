//
// Created by Daniel Dumas on 03/07/26.
//

#pragma once
#include <string>
#include <vector>

namespace iztadb::sstable {
    /**
     * @brief Returns the highest numbered log segment in the SST directory.
     *
     * @param sst_path Path to the SST directory.
     * @return Max segment number, 0 if directory is empty.
     */
    int calculate_latest_segment(const std::string& sst_path);

    /**
     * @brief Calculates CRC32 checksum for a SSTable block.
     *
     * Used to verify record integrity on recovery. Uses block_buffer bytes to check
     *
     * @param block_buffer member function
     * @param length the size in bytes of block_data.
     * @return CRC32 checksum of the record.
     */
    uint32_t calculate_crc32(const std::vector<uint8_t>& block_buffer);

}

