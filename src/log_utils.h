//
// Created by Daniel Dumas on 22/06/26.
//

#pragma once
#include <string>
#include "types.h"

namespace iztadb::wal {
    /**
     * @brief Returns the highest numbered log segment in the WAL directory.
     *
     * @param wal_path Path to the WAL directory.
     * @return Max segment number, 0 if directory is empty.
     */
    int calculate_latest_segment(const std::string& wal_path);

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
    uint32_t calculate_crc32(WALRecord record);

    constexpr uintmax_t MaxSegmentSize = 32 * 1024 * 1024;
}
