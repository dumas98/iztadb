//
// Created by Daniel Dumas on 22/06/26.
//

#pragma once

#include <string>

#include "memtable.h"

/**
 * @brief Manages read operations to the Write-Ahead Log (WAL).
 *
 * On construction, resolves the active log file by scanning the WAL
 * directory for the latest file (contains the highest number).
 *
 * IztaDB will send a MemTable to its restore function and the operations
 * stored in the WAL will be executed.
 *
 */
class LogReader
{
private:
    std::string wal_path;
    int latest_segment_num;

    /**
     * @brief Reads and replays all valid WAL records from a single segment file
     * into the provided MemTable.
     *
     * Iterates through each record in the segment sequentially, verifying CRC32
     * integrity before applying it. Stops at the first corrupted or partial record.
     *
     * @param path Absolute path to the segment file (e.g. /wal/000001.log).
     * @param mem_table The MemTable to restore records into.
     *
     * @return true if the segment was read cleanly to EOF.
     * @return false if a corrupted or partial record was found.
     */
    bool restore_segment(const std::string& path, MemTable& mem_table);

public:
    /**
     * @brief Constructs the LogReader and initializes WAL state.
     *
     * Resolves the active log segment.
     *
     * @param wal_path Path to the WAL directory. Defaults to "./data/wal".
     */
    explicit LogReader(const std::string& wal_path = "./data/wal");

    /**
     * @brief Replays all WAL segments in order to reconstruct the MemTable
     *
     * Iterates through each segment in the WAL directory.
     * Stops immediately at the first corrupted or partial record inside any
     * segment. Segments and/or missing records after the corruption point are
     * not processed since their state was never durably committed.
     *
     * Should be called once during database open, before accepting any
     * new reads or writes.
     *
     * @param mem_table  The MemTable to restore into.
     *
     */
    void restore_mem_table(MemTable& mem_table);

    /**
     * @brief Returns the WAL directory path.
     * @return Path to the WAL directory.
     */
    std::string get_wal_path();

    /**
     * @brief Returns the highest numbered log segment.
     * @return Max segment number, 0 if no segments exist.
     */
    int get_latest_segment_num();

};