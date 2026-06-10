//
// Created by Daniel Dumas on 05/06/26.
//

#include "log_writer.h"
using namespace std;

LogWriter::LogWriter() {
    write_path = resolve_active_log_path();
    next_sequence = get_next_sequence();
}

int LogWriter::get_max_segment() {
    string wal_path = "./data/wal";
    int max_file = 0;

    // No files exist, use zero.
    if (filesystem::is_empty(wal_path)) {
        return max_file;
    }

    // Iterate through each directory to get max (corresponds to latest file).
    for (auto const& dir_entry : filesystem::directory_iterator(wal_path)) {
        // Filter out non .log files.
        if (dir_entry.path().extension() != ".log") continue;

        // Get only file name without extension with stem() and convert to integer.
        int num = std::stoi(dir_entry.path().stem());

        if (num > max_file) {
            max_file = num;
        }
    }

    return max_file;
}

string LogWriter::resolve_active_log_path(uintmax_t max_size) {
    string wal_path = "./data/wal";
    int max_file = get_max_segment();

    // No files exist, start fresh.
    if (max_file == 0) {
        return wal_path + "/000001.log";
    }

    // Build latest file path.
    string latest_file_path = wal_path + "/" + format("{:06d}.log", max_file);

    // Return latest file path if file size is smaller than threshold.
    if (filesystem::file_size(latest_file_path) < max_size) {
        return latest_file_path;
    }

    // Add a one to max file so a new file can be created.
    return wal_path + "/" + format("{:06d}.log", max_file + 1);
}

string LogWriter::get_write_path() {
    return write_path;
};

uint32_t LogWriter::get_next_sequence() {
    int max_file = get_max_segment();

    // If no files exist, start at 1.
    if (max_file == 0) {
        return 1;
    }

    // Open latest segment.
    string latest_path = "./data/wal/" + format("{:06d}.log", max_file);
    ifstream file(latest_path, ios::binary);

    // Throw error if opening file fails.
    if (!file.is_open()) {
        throw runtime_error("Failed to open WAL file: " + latest_path);
    }

    // Jump to start of last record, convert first to signed int.
    file.seekg(-static_cast<int>(sizeof(WALRecord)), ios::end);

    // Convert sequence address to char*.
    // Read only the sequence number (first 4 bytes).
    uint32_t sequence_number;
    file.read(reinterpret_cast<char*>(&sequence_number),sizeof(uint32_t));

    return sequence_number + 1;
}

uint32_t LogWriter:: calculate_crc32(WALRecord record) {
    // Set to zero so contents match when reading.
    record.checksum = 0;

    // Start from 0, insert the address of record copy and scan the size of WALRecord.
    return crc32(0, reinterpret_cast<const Bytef*>(&record), sizeof(WALRecord));
};

void LogWriter::write_record(const std::string& key, const std::string& value, ValueType type) {
    // Assign data for individual record.
    WALRecord record;
    record.sequence_number = next_sequence;
    record.type = static_cast<uint8_t>(type);

    // Copies raw bytes from key to record.key, uses null terminator ('\0') for unoccupied bytes.
    // c_str() is const char* pointing to first char of String.
    // Repeats for record.value.
    strncpy(record.key, key.c_str(), sizeof(record.key));
    strncpy(record.value, value.c_str(), sizeof(record.value));

    // Assign checksum value after record has all data.
    record.checksum =  calculate_crc32(record);

    // Open file, binary mode, output operations happen at the end.
    ofstream file(write_path, ios::binary | ios::app);

    // Write to file record data, converting to char* WALRecord*
    file.write(reinterpret_cast<char*>(&record), sizeof(WALRecord));
    file.close();

    // Add one to next_sequence attribute so it's use for next write record.
    next_sequence ++;

    // After record is written recalculate write path.
    write_path = resolve_active_log_path();
};
