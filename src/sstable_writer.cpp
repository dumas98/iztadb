//
// Created by Daniel Dumas on 03/07/26.
//

#include "sstable_writer.h"
#include "sstable_utils.h"
#include "memtable.h"
#include "types.h"
#include <string>
#include <format>


SSTableWriter::SSTableWriter(const std::string& sst_path, uintmax_t block_size) {
    this->sst_path = sst_path;
    this->block_size = block_size;
    latest_segment_num = iztadb::sstable::calculate_latest_segment(sst_path);
    // Write path is latest_segment_num plus one.
    write_path = sst_path + "/" + std::format("{:06d}.sst", latest_segment_num + 1);
}

bool SSTableWriter::flush_mem_table(const MemTable& mem_table) {
    // Write to this writer's own auto-numbered path.
    if (!flush_map(mem_table.get_map(), write_path)) return false;

    // Recalculate write path
    latest_segment_num++;
    write_path = sst_path + "/" + std::format("{:06d}.sst", latest_segment_num + 1);

    return true;
}

bool SSTableWriter::flush_map(const std::map<std::string, Entry>& records, const std::filesystem::path& out_path) {

    // Start from blank state.
    index_entries.clear();
    block_buffer.clear();

    // Open new file.
    file.open(out_path, std::ios::binary);
    if (!file.is_open()) return false;

    // Declare counters to keep track of movements inside file.
    uint32_t block_counter = 0;
    uintmax_t segment_counter = 0;
    std::string last_key;

    // Write records for each key-value pair.
    for (const auto& [key, entry] : records) {
        build_block_buffer(key, entry, block_counter);
        last_key = key;
        // Stop Block when it's larger than 4KB.
        if (block_counter > block_size) {
            close_block(block_counter, segment_counter, last_key);
        }
    }

    // Close the remaining records that didn't reach threshold.
    if (block_counter > 0) {
        close_block(block_counter, segment_counter, last_key);
    }

    // Write index to file.
    write_index();

    // Write footer.
    uintmax_t index_offset = segment_counter;
    write_footer(index_offset);
    file.close();

    return true;
}

void SSTableWriter::build_block_buffer(const std::string& key, const Entry& entry, uint32_t& block_counter) {
    // Append bytes to buffer after writing each part of record.
    // Write Key.
    uint32_t key_size = static_cast<uint32_t>(key.size());
    append_bytes(&key_size, sizeof(key_size));
    append_bytes(key.data(), key_size);

    // Write ValueType.
    uint8_t type = static_cast<uint8_t>(entry.type);
    append_bytes(&type, sizeof(type));

    // Write Value.
    uint32_t value_size = static_cast<uint32_t>(entry.value.size());
    append_bytes(&value_size, sizeof(value_size));
    append_bytes(entry.value.data(), value_size);

    // Buffer size equals block counter.
    block_counter = static_cast<uint32_t>(block_buffer.size());
}

void SSTableWriter::append_bytes(const void* data, size_t data_len) {
    // Convert to bytes so any call can pass any type of pointer.
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    // Insert to buffer.
    block_buffer.insert(block_buffer.end(), bytes, bytes + data_len);
}

void SSTableWriter::close_block(uint32_t& block_counter, uintmax_t& segment_counter, const std::string& last_key) {
    // Calculate Checksum.
    uint32_t checksum = iztadb::sstable::calculate_crc32(block_buffer);

    // Write record using block_buffer.
    file.write(reinterpret_cast<const char*>(block_buffer.data()), block_buffer.size());

    // Write Checksum and add inserted bytes to block counter.
    file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    block_counter += sizeof(checksum);

    // Insert to index entries IndexEntry struct with relevant fields.
    index_entries.push_back({last_key, segment_counter, block_counter});

    // Add to block_count to overall file inserted bytes and restart counter and buffer for next block.
    segment_counter += block_counter;
    block_counter = 0;
    block_buffer.clear();
}

uintmax_t SSTableWriter::write_index() {

    // Iterate through each IndexEntry and write to file
    for (const auto& index_entry : index_entries) {
        uint32_t key_len = static_cast<uint32_t>(index_entry.last_key.size());
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(index_entry.last_key.data(), key_len);
        file.write(reinterpret_cast<const char*>(&index_entry.offset), sizeof(index_entry.offset));
        file.write(reinterpret_cast<const char*>(&index_entry.size), sizeof(index_entry.size));
    }
}

void SSTableWriter::write_footer(uintmax_t index_offset) {
    file.write(reinterpret_cast<const char*>(&index_offset), sizeof(index_offset));
    file.write(reinterpret_cast<const char*>(&iztadb::sstable::kMagic), sizeof(iztadb::sstable::kMagic));
}

std::filesystem::path SSTableWriter::get_sstable_path() {
    return sst_path;
}

uintmax_t SSTableWriter::get_block_size() {
    return block_size;
}

std::filesystem::path SSTableWriter::get_write_path() {
    return write_path;
}

int SSTableWriter::get_latest_segment_num() {
    return latest_segment_num;
}

std::vector<uint8_t> SSTableWriter::get_block_buffer() {
    return block_buffer;
}

std::vector<IndexEntry> SSTableWriter::get_index_entries() {
    return index_entries;
}



