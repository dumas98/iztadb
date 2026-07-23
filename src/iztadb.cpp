//
// Created by Daniel Dumas on 05/06/26.
//

// iztadb.cpp
#include "iztadb.h"

IztaDB::IztaDB(const std::filesystem::path& wal_path, const std::filesystem::path& sst_path,
    uintmax_t memtable_threshold, uintmax_t max_wal_file_size) {
    this->wal_path = wal_path;
    this->sst_path = sst_path;
    this->memtable_threshold = memtable_threshold;
    this->max_wal_file_size = max_wal_file_size;

    // Build MemTable.
    mem_table = std::make_unique<MemTable>();

    std::filesystem::create_directories(wal_path);

    // Instantiate LogReader, restore MemTable and fix WAL according using
    // the RecoveryResult. If it's empty and no files are found in WAL, it won't
    // touch it and will return its default RecoveryResult {true, true, "", 0}.
    LogReader log_reader(wal_path.string());
    log_reader.restore_mem_table(*mem_table);
    apply_recovery_result(log_reader.get_recovery_result());

    // LogWriter resumes from wherever the (possibly truncated) WAL now ends.
    log_writer = std::make_unique<LogWriter>(wal_path.string(), max_wal_file_size);

    sstable_writer = std::make_unique<SSTableWriter>(sst_path, 4096);

    // Load existing SSTables to member.
    load_sstables();
}

void IztaDB::load_sstables() {
    std::filesystem::create_directories(sst_path);

    // Collect all paths from SST directory.
    std::vector<std::filesystem::path> sst_files;
    for (const auto& entry : std::filesystem::directory_iterator(sst_path)) {
        if (entry.path().extension() == ".sst") {
            sst_files.push_back(entry.path());
        }
    }

    // Sort file names.
    std::sort(sst_files.begin(), sst_files.end());

    // Add all SSTableReaders to member.
    for (const auto& path : sst_files) {
        sstable_readers.push_back(std::make_unique<SSTableReader>(path));
    }
}

void IztaDB::apply_recovery_result(const RecoveryResult& result) {
    // Clean result means no change to WAL directory.
    if (result.clean) {
        return;
    }

    // Resize latest file to its last clean offset.
    std::filesystem::resize_file(result.segment, result.last_clean_offset);

    // Get the latest clean file num.
    int flagged_segment_num = std::stoi(std::filesystem::path(result.segment).stem().string());

    // Delete all files written after latest clean record (those greater than the flagged segment).
    for (const auto& entry : std::filesystem::directory_iterator(wal_path)) {
        if (entry.path().extension() != ".log") continue;
        int segment_num = std::stoi(entry.path().stem().string());
        if (segment_num > flagged_segment_num) {
            std::filesystem::remove(entry.path());
        }
    }
}

void IztaDB::put(const std::string& key, const std::string& value) {
    // Write to WAL first, durable before it touches memory.
    log_writer->write_record(key, value, ValueType::VALUE);

    // Insert to MemTable.
    mem_table->put(key, value);

    // Flush if the threshold was crossed.
    maybe_flush();
}

void IztaDB::remove(const std::string& key) {
    // Write to WAL first, durable before it touches memory.
    log_writer->write_record(key, "", ValueType::TOMBSTONE);

    // Insert to MemTable writing a tombstone record.
    mem_table->remove(key);

    // Flush if the threshold was crossed.
    maybe_flush();
}

void IztaDB::maybe_flush() {

    // Check if mem_table exceeds size threshold.
    if (mem_table->get_map().size() < memtable_threshold) {
        return;
    }

    // Swap in a fresh MemTable, move data to old_memtable.
    std::unique_ptr<MemTable> old_memtable = std::move(mem_table);
    mem_table = std::make_unique<MemTable>();

    // Flush MemTable.
    bool ok = sstable_writer->flush_mem_table(*old_memtable);

    if (ok) {
        // Add new data to the SSTable readers.
        sstable_readers.push_back(std::make_unique<SSTableReader>(sstable_writer->get_write_path()));

        // Clear WAL directory.
        for (const auto& entry : std::filesystem::directory_iterator(wal_path)) {
            if (entry.path().extension() == ".log") {
                std::filesystem::remove(entry.path());
            }
        }
        // Delete log_writer and replace with a new one.
        log_writer = std::make_unique<LogWriter>(wal_path.string(), max_wal_file_size);
    } else {
        // Flush failed, get data back to MemTable from the old_memtable previously built.
        for (const auto& [key, entry] : old_memtable->get_map()) {
            if (entry.type == ValueType::TOMBSTONE) {
                mem_table->remove(key);
            } else {
                mem_table->put(key, entry.value);
            }
        }
    }
}

std::optional<std::string> IztaDB::get(const std::string& key) {
    // Check MemTable first.
    if (auto value = mem_table->get(key)) {
        return value;
    }

    // Check if it exists in SSTables, newest to oldest.
    for (auto it = sstable_readers.rbegin(); it != sstable_readers.rend(); ++it) {
        GetResult result = (*it)->get(key);

        // Only stop if it found a value deleted or existing.
        if (result.status == LookupResult::FOUND) {
            return result.value;
        }
        if (result.status == LookupResult::DELETED) {
            return std::nullopt;
        }
    }

    // Exhausted every file without a match.
    return std::nullopt;
}