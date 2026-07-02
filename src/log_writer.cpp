//
// Created by Daniel Dumas on 05/06/26.
//

#include "log_writer.h"
#include "log_utils.h"
#include <iostream>
using namespace std;

LogWriter::LogWriter(const std::string& wal_path, uintmax_t max_wal_file_size) {
    this->wal_path = wal_path;
    this->max_wal_file_size = max_wal_file_size;
    latest_segment_num = iztadb::wal::calculate_latest_segment(wal_path);
    write_path = resolve_active_log_path();
    next_sequence = calculate_next_sequence();
}


string LogWriter::resolve_active_log_path() {
    // No files exist, start fresh.
    if (latest_segment_num == 0) {
        return wal_path + "/000001.log";
    }

    // Build latest file path.
    string latest_file_path = wal_path + "/" + format("{:06d}.log", latest_segment_num);

    // Return latest file path if file size is smaller than threshold.
    if (filesystem::file_size(latest_file_path) < max_wal_file_size) {
        return latest_file_path;
    }

    // Add a one to max file so a new file can be created.
    return wal_path + "/" + format("{:06d}.log", latest_segment_num + 1);
}

uint32_t LogWriter::calculate_next_sequence() {

    // If no files exist, start at 1.
    if (latest_segment_num == 0) {
        return 1;
    }

    // Open latest segment.
    string latest_path = wal_path + format("{:06d}.log", latest_segment_num);
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

    file.close();

    return sequence_number + 1;
}


void LogWriter::write_record(const std::string& key, const std::string& value, ValueType type) {
    // Make a copy of write path.
    string write_path_temp = write_path;

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
    record.checksum =  iztadb::wal::calculate_crc32(record);

    // Open file, binary mode, output operations happen at the end.
    ofstream file(write_path, ios::binary | ios::app);

    // Write to file record data, converting to char* WALRecord*
    file.write(reinterpret_cast<char*>(&record), sizeof(WALRecord));
    file.close();

    // Add one to next_sequence attribute so it's used for next write record.
    next_sequence ++;

    // Recalculate latest segment.
    latest_segment_num = iztadb::wal::calculate_latest_segment(wal_path);

    // After record is written recalculate write path.
    write_path = resolve_active_log_path();

    // EOF was reached, add a CLOSE record type to EOF.
    if (write_path != write_path_temp) {
        // Write close record to previous file.
        ofstream file(write_path_temp, ios::binary | ios::app);
        WALRecord close_record = {};
        close_record.type = static_cast<uint8_t>(ValueType::CLOSE);
        close_record.checksum = iztadb::wal::calculate_crc32(close_record);

        // Write to file.
        file.write(reinterpret_cast<char*>(&close_record), sizeof(WALRecord));
    }
};

string LogWriter::get_wal_path() {
    return wal_path;
}

uintmax_t LogWriter::get_max_wal_file_size() {
    return max_wal_file_size;
}

string LogWriter::get_write_path() {
    return write_path;
}

uint32_t LogWriter::get_next_sequence() {
    return next_sequence;
}

int LogWriter::get_latest_segment_num() {
    return latest_segment_num;
}