//
// Created by Daniel Dumas on 05/06/26.
//

#include "log_writer.h"
using namespace std;

LogWriter::LogWriter() {
    write_path = resolve_log_path();
}

string LogWriter::resolve_log_path() {

    string wal_path = "./data/wal";
    int max_file = 0;

    // If directory is empty, return first log file.
    if (filesystem::is_empty(wal_path)) {
        return wal_path + "/000001.log";
    }

    // Iterate through each directory to get max (corresponds to latest file).
    for (auto const& dir_entry : filesystem::directory_iterator(wal_path)){

        // Filter out non .log files.
        if (dir_entry.path().extension() != ".log") continue;

        // Get only file name without extension with stem() and convert to integer.
        int num = std::stoi(dir_entry.path().stem());

        if (num > max_file) {
            max_file = num;
        }
    }

    // Build latest file path.
    string latest_file_path = wal_path + "/" + format("{:06d}.log", max_file);

    // Return latest file path if file size is smaller than threshold.
    if (filesystem::file_size(latest_file_path) < MAX_WAL_FILE_SIZE) {
        return latest_file_path;
    }

    // Add a one to max file so a new file can be created.
    return wal_path + "/" + format("{:06d}.log", max_file + 1);

}

string LogWriter::get_write_path() {
    return write_path;
};
