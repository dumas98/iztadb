//
// Created by Daniel Dumas on 05/06/26.
//

// iztadb.cpp
#include "iztadb.h"
#include <iostream>

IztaDB::IztaDB(const std::filesystem::path& wal_path, const std::filesystem::path& sst_path,
    uintmax_t memtable_threshold, uintmax_t max_wal_file_size, uintmax_t block_size,
    uintmax_t compaction_trigger) {
    this->wal_path = wal_path;
    this->sst_path = sst_path;
    this->memtable_threshold = memtable_threshold;
    this->max_wal_file_size = max_wal_file_size;
    this->block_size = block_size;
    this->compaction_trigger = compaction_trigger;

    // Build MemTable.
    mem_table = std::make_unique<MemTable>();

    std::filesystem::create_directories(wal_path);
    std::filesystem::create_directories(sst_path);



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
        uintmax_t just_written_segment = sstable_writer->get_latest_segment_num();
        std::filesystem::path just_written_path = sst_path / std::format("{:06d}.sst", just_written_segment);
        sstable_readers.push_back(std::make_unique<SSTableReader>(just_written_path));

        // Clear WAL directory.
        for (const auto& entry : std::filesystem::directory_iterator(wal_path)) {
            if (entry.path().extension() == ".log") {
                std::filesystem::remove(entry.path());
            }
        }
        // Delete log_writer and replace with a new one.
        log_writer = std::make_unique<LogWriter>(wal_path.string(), max_wal_file_size);

        // A flush is the only thing that adds an SSTable, so this is the only
        // place the file count can cross the trigger.
        maybe_compact();

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
    // Get map from MemTable and search for key.
    const auto& map = mem_table->get_map();
    auto it = map.find(key);

    // If found value.
    if (it != map.end()) {
        // Key was deleted.
        if (it->second.type == ValueType::TOMBSTONE) {
            return std::nullopt;
        }
        // Send newest data
        return it->second.value;
    }

    // Key absent from MemTable, search inside SSTables.
    for (auto rit = sstable_readers.rbegin(); rit != sstable_readers.rend(); ++rit) {
        GetResult result = (*rit)->get(key);
        if (result.status == LookupResult::FOUND) return result.value;
        if (result.status == LookupResult::DELETED) return std::nullopt;
    }
    return std::nullopt;
}

void IztaDB::maybe_compact() {
    // A trigger of zero turns automatic compaction off. compact() can still be
    // called directly, which is what the scaling benchmarks rely on to build up
    // many files and measure read amplification.
    if (compaction_trigger == 0) return;

    // Not enough files have piled up yet.
    if (sstable_readers.size() < compaction_trigger) return;

    compact();
}

bool IztaDB::compact() {
    // Nothing on disk to merge.
    if (sstable_readers.empty()) return false;

    // Snapshot input paths. load_sstables() sorted the readers ascending by
    // segment number, so this vector is in oldest to newest order.
    std::vector<std::filesystem::path> inputs;
    for (const auto& reader : sstable_readers) {
        inputs.push_back(reader->get_sst_path());
    }

    // Output takes a fresh segment number, computed while every input still
    // exists, so it outranks all of them until they are removed.
    int output_segment = iztadb::sstable::calculate_latest_segment(sst_path.string()) + 1;
    std::filesystem::path out_path = sst_path / std::format("{:06d}.sst", output_segment);
    std::filesystem::path tmp_path = sst_path / std::format("{:06d}.sst.tmp", output_segment);

    // Merge every record into one map, oldest file first so newer records
    // overwrite older ones for the same key. A tombstone erases instead of being
    // stored, which both applies the delete and drops the marker from the output.
    std::map<std::string, Entry> merged;
    for (const auto& reader : sstable_readers) {
        for (const auto& [key, entry] : reader->read_all_records()) {
            if (entry.type == ValueType::TOMBSTONE) {
                merged.erase(key);
            } else {
                merged[key] = entry;
            }
        }
    }

    // Reading is finished, release the input file handles.
    sstable_readers.clear();

    // Write under a temporary name. load_sstables() only picks up .sst files, so
    // a partially written .tmp left by a crash here is invisible and the inputs
    // are still authoritative.
    SSTableWriter compaction_writer(sst_path.string(), block_size);
    if (!compaction_writer.flush_map(merged, tmp_path)) {
        // Nothing was destroyed, reopen the inputs and report failure.
        load_sstables();
        return false;
    }

    // Publish the merged file, rename is atomic within a directory.
    std::filesystem::rename(tmp_path, out_path);

    // Delete oldest first. If this is interrupted the survivors are always a
    // suffix of the inputs, and a suffix can never hold an older record for a key
    // whose newer record was already deleted, so dropped tombstones stay safe.
    for (const auto& input : inputs) {
        std::filesystem::remove(input);
    }

    // Reopen over the single merged file.
    load_sstables();

    // The writer cached latest_segment_num when it was built and is now stale.
    // Left alone, its next flush would target the segment compaction just wrote
    // and overwrite every compacted record.
    sstable_writer = std::make_unique<SSTableWriter>(sst_path.string(), block_size);

    return true;
}

std::filesystem::path IztaDB::get_wal_path() {
    return wal_path;
}

std::filesystem::path IztaDB::get_sst_path() {
    return sst_path;
}

uintmax_t IztaDB::get_memtable_threshold() {
    return memtable_threshold;
}

uintmax_t IztaDB::get_max_wal_file_size() {
    return max_wal_file_size;
}

uintmax_t IztaDB::get_block_size() {
    return block_size;
}

uintmax_t IztaDB::get_compaction_trigger() {
    return compaction_trigger;
}
