//
// Created by Daniel Dumas on 29/05/26.
//

#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "memtable.h"
#include "log_writer.h"
#include "log_reader.h"
#include "sstable_writer.h"
#include "sstable_reader.h"
#include "sstable_utils.h"

/**
 * @brief Core DB engine wiring MemTable, WAL, and SSTables together.
 *
 * On construction, recovers the MemTable from the WAL (if any exists),
 * applies the recovery algorithm returning the result from recovery
 * and restores the WAL up to the cleanest record if data was not clean.
 * SSTables for each .sst file are stored.
 *
 */
class IztaDB {
private:
    std::filesystem::path wal_path;
    std::filesystem::path sst_path;
    uintmax_t memtable_threshold;
    uintmax_t max_wal_file_size;
    uintmax_t block_size;
    uintmax_t compaction_trigger;

    std::unique_ptr<MemTable> mem_table;
    std::unique_ptr<LogWriter> log_writer;
    std::unique_ptr<SSTableWriter> sstable_writer;
    std::vector<std::unique_ptr<SSTableReader>> sstable_readers;

public:
    /**
     * @brief Builds an IztaDB instance at the given paths.
     *
     * Recovery sequence: build an empty MemTable, restore it from the
     * WAL, apply the recovery algorithm and use the returning result
     * if it was flagged unclean to restore WAL, open a LogWriter continuing
     * from wherever the recovered WAL ends, then load every existing SSTable
     * files into an SSTableReader.
     *
     * @param wal_path Directory for WAL segments.
     * @param sst_path Directory for SSTable files.
     * @param memtable_threshold Byte threshold that triggers a flush.
     * @param max_wal_file_size Byte threshold for WAL segment rotation.
     * @param block_size Byte threshold that triggers block rotation.
     * @param compaction_trigger Number of SSTable files that triggers an
     * automatic compaction after a flush. 0 disables it, leaving compact()
     * available to call by hand.
     */
    explicit IztaDB(
        const std::filesystem::path& wal_path = "./data/wal",
        const std::filesystem::path& sst_path = "./data/sst",
        uintmax_t memtable_threshold = 4 * 1024 * 1024,
        uintmax_t max_wal_file_size = 32 * 1024 * 1024,
        uintmax_t block_size = 4096,
        uintmax_t compaction_trigger = 10
    );

    /**
     * @brief Writes a key-value pair.
     *
     * The record is written to the WAL first, then applied to the MemTable.
     * Triggers a flush if the MemTable exceeds memtable_threshold.
     *
     */
    void put(const std::string& key, const std::string& value);

    /**
     * @brief Deletes a key, using a tombstone.
     *
     * Similar to the put method. The record is written to the WAL first,
     * then applied to the MemTable. Triggers a flush if the MemTable exceeds
     * memtable_threshold.
     *
     */
    void remove(const std::string& key);

    /**
     * @brief Looks up a key's current value.
     *
     * Checks the live MemTable first. If absent there, checks SSTables from
     * newest to oldest. A FOUND or DELETED result from any file is final and
     * stops the search immediately. NOT_FOUND means keep searching in subsequent
     * files.
     *
     * @return The value if a live record exists anywhere or nullopt if
     * the key was never written, or was deleted.
     */
    std::optional<std::string> get(const std::string& key);

    /**
     * @brief Merges every SSTable on disk into a single compacted file.
     *
     * Reads all records from every SSTable oldest to newest into one map, so a
     * newer record overwrites an older one for the same key. Tombstones erase
     * their key instead of being stored, dropping them from the output, which is
     * only safe because this merges every live file, leaving nothing older behind
     * to resurrect a deleted key.
     *
     * The merged file is written under a temporary name and renamed into place
     * before any input is removed, and inputs are then deleted oldest first.
     * Whatever survives an interrupted run is a suffix of the original files,
     * which can never hold an older record for a key whose newer record is gone.
     *
     * Holds the entire live dataset in memory for the duration.
     *
     * @return true if a compaction ran, false if there were no SSTables to merge
     * or the merged file could not be written.
     */
    bool compact();

    /**
         * @brief Returns the WAL directory this instance was configured with.
         * @return WAL directory path.
         */
    std::filesystem::path get_wal_path();

    /**
     * @brief Returns the SSTable directory this instance was configured with.
     * @return SSTable directory path.
     */
    std::filesystem::path get_sst_path();

    /**
     * @brief Returns map threshold that triggers a flush.
     * @return MemTable threshold.
     */
    uintmax_t get_memtable_threshold();

    /**
     * @brief Returns the byte threshold that triggers WAL segment rotation.
     * @return Max WAL file size, in bytes.
     */
    uintmax_t get_max_wal_file_size();

    /**
     * @brief Returns the block threshold that triggers block rotation.
     * @return Block threshold in bytes.
     */
    uintmax_t get_block_size();

    /**
     * @brief Returns the SSTable file count that triggers an automatic compaction.
     * @return Compaction trigger, 0 when automatic compaction is disabled.
     */
    uintmax_t get_compaction_trigger();

private:
    /**
     * @brief Scans sst_path for .sst files and opens an SSTableReader
     * for each, sorted oldest to newest by segment number.
     */
    void load_sstables();

    /**
     * @brief Applies the recovery algorithm when a WAL segment was
     * flagged as unclean.
     *
     * If result.clean is true, does nothing. Otherwise, truncates
     * the flagged segment to last_clean_offset and deletes
     * every higher numbered segment.
     *
     * @param result The RecoveryResult from LogReader::restore_mem_table.
     */
    void apply_recovery_result(const RecoveryResult& result);

    /**
     * @brief Flushes the active MemTable to a new SSTable if it has
     * crossed memtable_threshold.
     *
     * Swaps in a fresh MemTable first, so new writes are never blocked
     * on the flush. If the flush succeeds, opens an SSTableReader for
     * the new file and clears the WAL directory. If the flush fails,
     * the old MemTable's entries are merged back into the live MemTable.
     *
     */
    void maybe_flush();

    /**
     * @brief Runs a full compaction once the SSTable count reaches
     * compaction_trigger.
     *
     * Called after every successful flush, the only point where a new SSTable
     * appears. Without it files accumulate forever and every read that misses
     * has to scan all of them.
     *
     * Does nothing when compaction_trigger is 0, which disables automatic
     * compaction entirely.
     */
    void maybe_compact();

};
