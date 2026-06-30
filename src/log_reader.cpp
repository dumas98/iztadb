//
// Created by Daniel Dumas on 22/06/26.
//

#include "log_reader.h"
#include "log_utils.h"
#include <types.h>
#include <fstream>
#include <iostream>
using namespace std;

LogReader::LogReader(const std::string& wal_path) {
    this->wal_path = wal_path;
    latest_segment_num = iztadb::wal::calculate_latest_segment(wal_path);
    recovery_result = {true, true, "", 0};
}

RecoveryResult LogReader::restore_segment(const std::string& path, MemTable& mem_table) {

    // Open file.
    ifstream file(path, ios::binary);

    // Read each record of the segment until it reaches the end.
    WALRecord wal_record;

    // Tracks byte position of current record,
    size_t offset = 0;

    while (file.read(reinterpret_cast<char*>(&wal_record), sizeof(WALRecord))) {
        // Corrupted data, checksum integrity test fails.
        if (wal_record.checksum != iztadb::wal::calculate_crc32(wal_record)) {
            return {false,  false, path, offset};
        }

        // Reached EOF cleanly.
        if (static_cast<ValueType>(wal_record.type) == ValueType::CLOSE) {
            return {true,  true, path, offset};
        }

        // Execute put.
        if (static_cast<ValueType>(wal_record.type) == ValueType::VALUE) {
            mem_table.put(string(wal_record.key), string(wal_record.value));
        }

        // Execute remove.
        else if (static_cast<ValueType>(wal_record.type) == ValueType::TOMBSTONE) {
            mem_table.remove(string(wal_record.key));
        }

        // Advance after successful record.
        offset += sizeof(WALRecord);
    }

    // When gcount() == 0, no more records to read. If gcount() != 0, partial data.
    if (file.gcount() > 0) {
        return {false,  false, path, offset};
    }

    // No EOF but potentially
    return {true,  false, path, offset};

}

void LogReader::restore_mem_table(MemTable& mem_table) {
    // Iterate through each segment.
    for (int i = 1; i <= latest_segment_num; i++) {
        // Current segment.
        string segment = wal_path + "/" + format("{:06d}.log", i);

        // Restore segment.
        RecoveryResult restored = restore_segment(segment, mem_table);

        // Check if non-latest segment data reached EOF.
        if ((i < latest_segment_num) && (!restored.reached_eof)) {
            restored.clean = false;
        }

        recovery_result = restored;

        // Stop if a corrupted or partial write record was found, change recovery_result.
        if (!restored.clean) {
            break;
        }
    }
}

string LogReader::get_wal_path() {
    return wal_path;
}

int LogReader::get_latest_segment_num() {
    return latest_segment_num;
}

RecoveryResult LogReader::get_recovery_result() {
    return recovery_result;
}