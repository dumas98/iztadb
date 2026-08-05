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
     * @return Max segment number, 0 if the directory is empty or does not exist.
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

    /**
     * @brief Fixed sentinel written as the last 4 bytes of every SSTable file.
     *
     * "IZDB" in ASCII. A reader checks this after seeking to the footer,
     * before trusting anything else in it.
     *
     */
    constexpr uint32_t kMagic = 0x495A4442;

    /**
     * @brief Total size, in bytes, of the fixed footer at the end of every
     * SSTable segment: [index_offset][kMagic].
     *
     * A reader locates the footer at file_size - kFooterSize.
     */
    constexpr size_t kFooterSize = sizeof(uintmax_t) + sizeof(kMagic);

}

/**
 * @brief One sparse index entry: a data block's boundary key, offset, and size.
 * One entry per block, not per key.
 */
struct IndexEntry {
    std::string last_key; // Last key written into this block.
    uintmax_t offset; // Byte position where the block starts.
    uint32_t size; // Block size in bytes, including its CRC32 checksum.
};

/**
 * @brief The outcome of looking up a key in one SSTable file.
 *
 */
enum class LookupResult {
    FOUND, // A live PUT record matched the key.
    DELETED, // A tombstone matched the key.
    NOT_FOUND // No record for this key exists in this file.
};

/**
 * @brief The result of an SSTableReader::get() call.
 *
 * value is only meaningful when status == FOUND.
 */
struct GetResult {
    LookupResult status; // Which of the three outcomes this is.
    std::optional<std::string> value; // The stored value, present only when status == FOUND.
};