//
// Created by Daniel Dumas on 03/07/26.
//

#pragma once
#include <string>
#include "memtable.h"
#include <fstream>

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
 * @brief Writes an immutable, sorted SSTable file to disk from a flushed MemTable.
 *
 * An SSTableWriter takes a MemTable's sorted contents and serializes them
 * into a single on-disk file laid out as:
 *
 *   Data Blocks
 *   [Records, CRC32 Checksum] -> These are ordered by key.
 *   Record Layout
 *   [Key Length, Key, Value Length, Value]
 *   Index Block
 *   [one entry per data block -> (last_key, offset, size)]
 *   Footer
 *   [fixed-size: points to the index]
 *
 * A record is never split across two blocks, so blocks may end up slightly
 * larger than block_size. The file is written once, sequentially.
 * SSTables are immutable for their entire lifetime.
 */
class SSTableWriter {
    private:
        std::string sst_path;
        uint32_t block_size;
        std::string write_path;
        int latest_segment_num;
        std::ofstream file;
        std::vector<uint8_t> block_buffer;
        std::vector<IndexEntry> index_entries;
        /**
         * @brief Adds bytes to the block_buffer member tracking all bytes written to the segment
         * and used for CRC32 checksum.
         *
         * @param data address where bytes start.
         * @param data_len the end of range of the written bytes.
         */
        void append_bytes(const void* data, size_t data_len);

        /**
        * @brief Writes one key-value record to the open SSTable file and updates
        * the running byte count for the current block.
        *
        * Record Format: [key_size][key][type][value_size][value].
        * Tombstones just have value_size = 0, no special casing.
        *
        * Assumes file is already open, does not close the block.
        *
        * @param key The record's key.
        * @param entry The record's value and ValueType.
        * @param block_counter Running byte count for the current block; incremented
        * by the number of bytes written.
        */
        void build_block_buffer(const std::string& key, const Entry& entry, uint32_t& block_counter);


        /**
        * @brief Finalizes the current data block and prepares state for the next one.
        *
        * Appends a CRC32 checksum over the block's accumulated bytes, records an
        * IndexEntry using segment_counter's current value as this block's starting
        * offset, then advances segment_counter past this block and resets
        * block_counter for the next block.
        *
        * Order matters: the index entry must be pushed before segment_counter is
        * advanced, or it will record the wrong offset.
        *
        * @param block_counter Running byte count for the current block; reset to
        * 0 after this call.
        * @param segment_counter Running byte offset into the segment; on entry, holds
        * this block's starting offset, then advances to the next block's start.
        * @param last_key The latest key written into this block.
        */
        void close_block(uint32_t& block_counter, uintmax_t& segment_counter, const std::string& last_key);

        /**
         * @brief Serializes every recorded IndexEntry to the currently open SSTable
         * segment, after the last data block.
         *
         * Writes each entry as [key_len][last_key][offset][size]. Reads from
         * index_entries each IndexEntry in the order they were inserted.
         *
         */
         uintmax_t write_index();

    public:
        /**
         * @brief Constructs an SSTableWriter targeting a given output location.
         *
         * @param sst_path Directory where SSTable files are written. Default path "./data/sst"
         * @param block_size byte threshold where the writer closes the current data
         * block and starts a new one. Default = 4KB.
         */
        explicit SSTableWriter(
                    const std::string& sst_path = "./data/sst",
                    uintmax_t block_size = 4096 // 4KB.
                    );
        /**
         * @brief Flushes a MemTable's sorted contents into a new, immutable SSTable file.
         * @param mem_table Sorted MemTable that will be flushed to disk.
         * @return true if the SSTable file was fully written and fsynced;
         *         false if the flush failed. On false, partial output file
         *         should be treated as valid or readable.
         */
        bool flush_mem_table(const MemTable& mem_table);

        /**
        * @brief Writes the fixed footer that terminates every SSTable file.
        *
        * Layout: [index_offset (uintmax_t)][magic (uint32_t)], always the last
        * kFooterSize bytes.
        *
        * @param index_offset File offset where the index section begins.
        */
        void write_footer(uintmax_t index_offset);

        /**
         * @brief Returns the directory this SSTableWriter writes files into.
         * @return SSTable output directory.
         */
        std::filesystem::path get_sstable_path();

        /**
         * @brief Returns the byte threshold at which a data block is closed.
         * @return Configured block size, in bytes.
         */
        uintmax_t get_block_size();

        /**
         * @brief Returns the path of the file currently being (or last) written.
         * @return Current write path.
         */
        std::filesystem::path get_write_path();

        /**
         * @brief Returns the segment number that will be assigned to the next
         * SSTable file this writer produces.
         * @return Next segment number.
         */
        int get_latest_segment_num();

        /**
         * @brief Returns the in-memory buffer for the data block currently being built.
         * @return Current block buffer.
         */
        std::vector<uint8_t> get_block_buffer();

        /**
         * @brief Returns the index entries recorded so far for the file currently
         *        being flushed.
         * @return Accumulated index entries.
         */
        std::vector<IndexEntry> get_index_entries();

};
