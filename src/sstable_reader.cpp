//
// Created by Daniel Dumas on 21/07/26.
//

#include "sstable_utils.h"
#include "sstable_reader.h"
#include "types.h"

SSTableReader::SSTableReader(const std::filesystem::path& sst_path) {
    // Open file
    this->sst_path = sst_path;
    file_size = std::filesystem::file_size(sst_path);
    file.open(sst_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("SSTableReader: failed to open file: " + sst_path.string());
    }

    // Load footer and index_entries to members.
    load_footer();
    load_index();
}

void SSTableReader::load_footer() {
    // Footer is fixed size and always at the end.
    file.seekg(file_size - iztadb::sstable::kFooterSize);

    // Read index offset from footer.
    file.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    // Read magic.
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    // If magic isn't equal to set value, there was a corruption, return an error.
    if (magic != iztadb::sstable::kMagic) {
        throw std::runtime_error("SSTableReader: invalid magic number, corrupt or non-SSTable file: " + sst_path.string());
    }
}

void SSTableReader::load_index() {
    // Calculate index boundary.
    uintmax_t index_end = file_size - iztadb::sstable::kFooterSize;

    // Seek to start of index section.
    file.seekg(index_offset);

    // Parse entries until the cursor reaches the footer boundary.
    // Add each index entry as IndexEntry to the member index_entries.
    while (static_cast<uintmax_t>(file.tellg()) < index_end) {
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        std::string last_key(key_len, '\0');
        file.read(last_key.data(), key_len);

        uintmax_t offset;
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        index_entries.push_back({last_key, offset, size});
    }
}

std::optional<IndexEntry> SSTableReader::find_block(const std::string& key) {
    // Binary search the key in the indexes. Use lower_bound method and add a comparator
    // to do the test for each index. On th first block where last_key is not less than the target
    // the key becomes a candidate to be contained inside the block.
    auto it = std::lower_bound(index_entries.begin(), index_entries.end(), key,
        [](const IndexEntry& entry, const std::string& target) {
            return entry.last_key < target;
        });

    // Key sorts past every block, not in this file.
    if (it == index_entries.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<uint8_t> SSTableReader::verify_and_read_block(const IndexEntry& block) {

    // Create buffer and read block.
    std::vector<uint8_t> block_bytes(block.size);
    file.seekg(block.offset);
    file.read(reinterpret_cast<char*>(block_bytes.data()), block.size);

    // Extract the stored checksum.
    uint32_t stored_crc;
    std::memcpy(&stored_crc, block_bytes.data() + block.size - sizeof(uint32_t), sizeof(uint32_t));

    // Extract the data, recalculate checksum and verify they match.
    std::vector<uint8_t> data_only(block_bytes.begin(), block_bytes.end() - sizeof(uint32_t));
    if (iztadb::sstable::calculate_crc32(data_only) != stored_crc) {
        throw std::runtime_error("SSTableReader: block checksum mismatch: " + sst_path.string());
    }

    return data_only;
}

GetResult SSTableReader::scan_records(const std::vector<uint8_t>& data, const std::string& key) {
    // Position of the current record of buffer.
    size_t pos = 0;

    // Iterate through buffer, start in beginning.
    while (pos < data.size()) {

        // Extract record.
        uint32_t key_len;
        std::memcpy(&key_len, data.data() + pos, sizeof(key_len));
        pos += sizeof(key_len);

        std::string record_key(reinterpret_cast<const char*>(data.data() + pos), key_len);
        pos += key_len;

        uint8_t type;
        std::memcpy(&type, data.data() + pos, sizeof(type));
        pos += sizeof(type);

        uint32_t value_len;
        std::memcpy(&value_len, data.data() + pos, sizeof(value_len));
        pos += sizeof(value_len);

        // If record_key was found, check if it's tombstone and return deleted,
        // otherwise return FOUND and the actual value.
        if (record_key == key) {
            if (static_cast<ValueType>(type) == ValueType::TOMBSTONE) {
                return {LookupResult::DELETED, std::nullopt};
            }
            std::string value(reinterpret_cast<const char*>(data.data() + pos), value_len);
            return {LookupResult::FOUND, value};
        }
        // Since block is sorted, stop immediately when record_key is larger than candidate key,
        // key won't exist in file.
        if (record_key > key) {
            return {LookupResult::NOT_FOUND, std::nullopt};
        }

        // Not a match, skip this record's value, move to the next
        pos += value_len;
    }
    return {LookupResult::NOT_FOUND, std::nullopt};
}

GetResult SSTableReader::scan_block(const IndexEntry& block, const std::string& key) {
    // Do checksum test and get data from block as buffer.
    std::vector<uint8_t> data = verify_and_read_block(block);

    // Scan records of block.
    return scan_records(data, key);
}

GetResult SSTableReader::get(const std::string& key) {
    std::optional<IndexEntry> block = find_block(key);

    // Key exceeded last index boundary.
    if (!block.has_value()) {
        return {LookupResult::NOT_FOUND, std::nullopt};
    }

    // Return Result after scanning block.
    return scan_block(block.value(), key);
}