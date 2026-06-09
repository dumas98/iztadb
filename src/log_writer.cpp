//
// Created by Daniel Dumas on 05/06/26.
//

#include "log_writer.h"
using namespace std;

LogWriter::LogWriter() {
    write_path = resolve_active_log_path();
    sequence_number = get_latest_sequence();
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

uint32_t LogWriter::get_latest_sequence() {
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

    // Read only the sequence number (first 4 bytes), convert sequence address
    // to the first char (byte).
    uint32_t sequence_number;
    file.read(reinterpret_cast<char*>(&sequence_number),sizeof(uint32_t));

    return sequence_number + 1;
}
