//
// Created by Daniel Dumas on 20/07/26.
//

#pragma once
#include <fstream>
#include <utility>
#include "sstable_utils.h"
#include "types.h"

/**
 * @brief Reads a single SSTable file and returns the operation result.
 *
 * On construction, it loads the footer and the full sparse index into memory,
 * every get() then uses the indexes to find the candidate block via binary
 * search, and scans it to look for the value, returning either the value
 * for the key, if it was deleted or not found.
 *
 */
class SSTableReader {
    private:
        std::filesystem::path sst_path;
        std::ifstream file;
        uintmax_t file_size;
        std::vector<IndexEntry> index_entries;
        uintmax_t index_offset;

        /**
        * @brief Locates and reads the fixed footer at the end of the SSTable file.
        *
        * Seeks to file_size - kFooterSize, reads index_offset into the member,
        * then validates the magic constant before returning. Called once, from
        * the constructor.
        *
        * @throws std::runtime_error if the magic number doesn't match kMagic.
        */
        void load_footer();

        /**
         * @brief Reads every IndexEntry from the index section to member.
         *
         * Seeks to index_offset and parses entries in until the read
         * cursor reaches file_size - kFooterSize, the footer boundary.
         * Called once, from the constructor. Used for binary-searches
         * when looking for a block.
         */
        void load_index();

         /**
         * @brief Binary searches the index for the block that could
         * contain a given key.
         *
         * Finds the first IndexEntry whose last_key is not less than
         * key, the first block to possibly contain it.
         *
         * @param key The key to search for.
         * @return The candidate IndexEntry or std::nullopt if key
         * passes every boundary.
         */
        std::optional<IndexEntry> find_block(const std::string& key);

        /**
         * @brief Reads one data block from disk and verifies its checksum.
         *
         * Seeks to block.offset, reads record bytes, splits the CRC32 and
         * recomputes it with calculate_crc32.
         *
         * @param block The block to read.
         * @return The verified record data, with the CRC32 trailer removed.
         * @throws std::runtime_error if the recomputed checksum doesn't
         * match the stored one, the block is corrupted.
         */
        std::vector<uint8_t> verify_and_read_block(const IndexEntry& block);

        /**
             * @brief Reads, verifies, and scans one block for a key.
             *
             * @param block The candidate block.
             * @param key The key being looked up.
             * @return Whatever scan_records() returns for this block's data.
             * @throws std::runtime_error when verify_and_read_block() rejects
             * the block's checksum.
             */
        GetResult scan_block(const IndexEntry& block, const std::string& key);

        /**
         * @brief Scans linearly verified block data for an exact key match.
         *
         * Walks records sequentially, stops the moment a record's key
         * is past the target, since the block is sorted and nothing
         * further could match. A tombstone match returns DELETED without
         * reading toward a value that isn't there.
         *
         * @param data Verified record bytes (with checksum).
         * @param key The key being searched for.
         * @return FOUND with the value, DELETED if a tombstone matched,
         *         or NOT_FOUND if the scan reached the end without a match.
         */
    GetResult scan_records(const std::vector<uint8_t>& data, const std::string& key);

    public:
        /**
         * @brief Opens an SSTable file and loads its footer and index entries.
         *
         * @param sst_path Path to an existing .sst file.
         * @throws std::runtime_error if the file can't be opened, or if
         * load_footer() rejects it (bad magic number).
         */
        explicit SSTableReader(const std::filesystem::path& sst_path);

        /**
         * @brief Looks up a key in this SSTable file.
         *
         * Binary searches the index via find_block() to locate
         * the one candidate block, then reads and scans it via
         * scan_block().
         *
         * @param key The key to look up.
         * @return FOUND with the value, DELETED if a tombstone matched,
         * or NOT_FOUND if the key isn't in this file.
         * @throws std::runtime_error if the matched block's CRC32 check
         * fails, the block is corrupted.
         */
        GetResult get(const std::string& key);

        /**
         * @brief Reads every record in this SSTable, in ascending key order.
         *
         * Scans all blocks in index order, verifying each one's checksum, and
         * decodes every record it holds. Tombstones are returned as they are stored,
         * with ValueType::TOMBSTONE and an empty value.
         *
         * @return Every record as a (key, Entry) pair, ascending by key.
         * @throws std::runtime_error if any block's CRC32 check fails, a block
         * is corrupted.
         */
        std::vector<std::pair<std::string, Entry>> read_all_records();

        /**
         * @brief Returns the path of the file opened.
         * @return SSTable file path.
         */
        std::filesystem::path get_sst_path();

        /**
         * @brief Returns index entries.
         * @return index entries.
         */
        std::vector<IndexEntry> get_index_entries();

        /**
         * @brief Returns the byte offset where the index section begins.
         * @return Index offset.
         */
        uintmax_t get_index_offset();
};


