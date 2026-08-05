//
// Created by Daniel Dumas on 04/08/26.
//

#include <gtest/gtest.h>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <random>
#include <string>
#include "iztadb.h"
#include "sstable_reader.h"

/**
 * @brief Test fixture for CompactionTest.
 *
 * Uses a small MemTable threshold so a handful of puts forces a flush, which
 * makes the number of SSTables on disk predictable from the number of writes.
 */
class CompactionTest : public testing::Test {
protected:
    // maybe_flush() compares the MemTable's entry count against this, so five
    // distinct keys is exactly one flush.
    static constexpr uintmax_t kMemtableThreshold = 5;

    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./test_data/compaction/wal";
    std::string sst_path = "./test_data/compaction/sst";

    // Test helper, build a database with a given automatic compaction trigger.
    std::unique_ptr<IztaDB> make_db(uintmax_t compaction_trigger) {
        return std::make_unique<IztaDB>(wal_path, sst_path, kMemtableThreshold,
                                        32 * 1024 * 1024, 4096, compaction_trigger);
    }

    // Before Each.
    void SetUp() override {
        std::filesystem::remove_all("./test_data/");

        // Automatic compaction is off here, the tests below drive compact()
        // directly and need an exact, predictable file count.
        db = make_db(0);
    }

    // After Each.
    void TearDown() override {
        db.reset();
        std::filesystem::remove_all("./test_data/");
        std::cout << "Test Ended." << std::endl;
    }

    // Test helper, count files with a given extension in a directory.
    int count_files(const std::string& dir, const std::string& extension) {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == extension) ++count;
        }
        return count;
    }

    int count_sst_files() { return count_files(sst_path, ".sst"); }
    int count_wal_files() { return count_files(wal_path, ".log"); }

    // Test helper, total bytes held by every SSTable.
    uintmax_t total_sst_bytes() {
        uintmax_t total = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sst_path)) {
            if (entry.path().extension() == ".sst") {
                total += std::filesystem::file_size(entry.path());
            }
        }
        return total;
    }

    // Test helper, write exactly enough distinct keys to trigger one flush.
    void write_one_flush(const std::string& prefix, const std::string& value) {
        for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
            db->put(std::format("{}{:02d}", prefix, i), value);
        }
    }
};

// 1. Nothing to compact.
TEST_F(CompactionTest, CompactWithNoSSTablesReturnsFalse) {
    EXPECT_FALSE(db->compact());
    EXPECT_EQ(count_sst_files(), 0);
}

// 2. Merge correctness.
// Newer file wins for a key written twice.
TEST_F(CompactionTest, OverwrittenKeyKeepsNewestValue) {
    // First flush holds the stale value.
    db->put("target", "old");
    for (uintmax_t i = 1; i < kMemtableThreshold; ++i) db->put(std::format("a{:02d}", i), "v");

    // Second flush overwrites it.
    db->put("target", "new");
    for (uintmax_t i = 1; i < kMemtableThreshold; ++i) db->put(std::format("b{:02d}", i), "v");

    ASSERT_EQ(count_sst_files(), 2);
    ASSERT_TRUE(db->compact());

    EXPECT_EQ(count_sst_files(), 1);
    auto value = db->get("target");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "new");
}

// A key only ever written to the oldest file must survive the merge.
TEST_F(CompactionTest, KeyOnlyInOldestFileSurvives) {
    db->put("ancient", "value");
    for (uintmax_t i = 1; i < kMemtableThreshold; ++i) db->put(std::format("a{:02d}", i), "v");

    write_one_flush("b", "v");
    write_one_flush("c", "v");

    ASSERT_EQ(count_sst_files(), 3);
    ASSERT_TRUE(db->compact());

    auto value = db->get("ancient");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value");
}

// The output takes a fresh segment number, one past the highest input.
TEST_F(CompactionTest, OutputTakesNextSegmentNumber) {
    write_one_flush("a", "v");
    write_one_flush("b", "v");
    ASSERT_EQ(count_sst_files(), 2);

    ASSERT_TRUE(db->compact());

    EXPECT_EQ(count_sst_files(), 1);
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(sst_path) / "000003.sst"));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(sst_path) / "000001.sst"));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(sst_path) / "000002.sst"));
}

// 3. Tombstones.
// A delete survives the merge, and the tombstone itself is discarded.
TEST_F(CompactionTest, TombstoneIsDroppedNotJustApplied) {
    db->put("gone", "value");
    for (uintmax_t i = 1; i < kMemtableThreshold; ++i) db->put(std::format("a{:02d}", i), "v");

    db->remove("gone");
    for (uintmax_t i = 1; i < kMemtableThreshold; ++i) db->put(std::format("b{:02d}", i), "v");

    ASSERT_EQ(count_sst_files(), 2);
    ASSERT_TRUE(db->compact());

    EXPECT_FALSE(db->get("gone").has_value());

    // NOT_FOUND rather than DELETED proves no tombstone record was carried over,
    // a retained tombstone would report DELETED instead.
    SSTableReader reader(std::filesystem::path(sst_path) / "000003.sst");
    EXPECT_EQ(reader.get("gone").status, LookupResult::NOT_FOUND);
}

// Compacting a single file is still worth doing, it drops that file's tombstones.
TEST_F(CompactionTest, CompactSingleSSTableDropsTombstones) {
    // The tombstone overwrites the put in the MemTable, so these two calls leave
    // one entry behind, not two. Four more keys are what reaches the threshold.
    db->put("gone", "value");
    db->remove("gone");
    for (uintmax_t i = 0; i < kMemtableThreshold - 1; ++i) db->put(std::format("a{:02d}", i), "v");

    ASSERT_EQ(count_sst_files(), 1);
    ASSERT_TRUE(db->compact());
    ASSERT_EQ(count_sst_files(), 1);

    SSTableReader reader(std::filesystem::path(sst_path) / "000002.sst");
    EXPECT_EQ(reader.get("gone").status, LookupResult::NOT_FOUND);
    EXPECT_FALSE(db->get("gone").has_value());
}

// Everything deleted leaves a valid file holding zero records.
TEST_F(CompactionTest, AllRecordsDeletedProducesEmptyCompactedFile) {
    write_one_flush("key", "value");
    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) db->remove(std::format("key{:02d}", i));

    ASSERT_EQ(count_sst_files(), 2);
    ASSERT_TRUE(db->compact());
    ASSERT_EQ(count_sst_files(), 1);

    // A segment with no blocks is still a well formed file, the reader has to
    // open it and report nothing rather than reject it.
    std::filesystem::path merged = std::filesystem::path(sst_path) / "000003.sst";
    ASSERT_TRUE(std::filesystem::exists(merged));
    SSTableReader reader(merged);
    EXPECT_TRUE(reader.get_index_entries().empty());
    EXPECT_TRUE(reader.read_all_records().empty());

    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_FALSE(db->get(std::format("key{:02d}", i)).has_value());
    }
}

// 4. Interaction with the rest of the engine.
// Compaction only touches SSTables, the live MemTable is untouched.
TEST_F(CompactionTest, LiveMemTableSurvivesCompaction) {
    write_one_flush("flushed", "v");
    db->put("unflushed", "in-memtable");

    ASSERT_TRUE(db->compact());

    auto value = db->get("unflushed");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "in-memtable");
}

// The WAL protects the MemTable, which was never part of the merge, so
// compaction must not clear it the way maybe_flush() does.
TEST_F(CompactionTest, CompactionDoesNotClearWal) {
    write_one_flush("flushed", "v");
    db->put("unflushed", "in-memtable");

    int wal_before = count_wal_files();
    ASSERT_GT(wal_before, 0);

    ASSERT_TRUE(db->compact());

    EXPECT_EQ(count_wal_files(), wal_before);
}

// The stale writer trap: a flush after compaction must not reuse the segment
// number compaction just wrote.
TEST_F(CompactionTest, FlushAfterCompactionDoesNotClobberCompactedFile) {
    write_one_flush("a", "first");
    write_one_flush("b", "second");
    ASSERT_EQ(count_sst_files(), 2);

    ASSERT_TRUE(db->compact());
    ASSERT_EQ(count_sst_files(), 1);

    // Compaction produced 000003.sst. A writer still holding its pre-compaction
    // segment number would target 000003.sst again and destroy every merged record.
    write_one_flush("c", "third");
    EXPECT_EQ(count_sst_files(), 2);

    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_EQ(db->get(std::format("a{:02d}", i)).value_or(""), "first");
        EXPECT_EQ(db->get(std::format("b{:02d}", i)).value_or(""), "second");
        EXPECT_EQ(db->get(std::format("c{:02d}", i)).value_or(""), "third");
    }
}

// Reopening reads the compacted file and keeps numbering consistent.
TEST_F(CompactionTest, DataSurvivesReopenAfterCompaction) {
    write_one_flush("a", "first");
    write_one_flush("b", "second");
    ASSERT_TRUE(db->compact());

    db.reset();
    db = make_db(0);

    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_EQ(db->get(std::format("a{:02d}", i)).value_or(""), "first");
        EXPECT_EQ(db->get(std::format("b{:02d}", i)).value_or(""), "second");
    }
    EXPECT_EQ(count_sst_files(), 1);
}

// Compacting an already compacted database is a no-op that stays correct.
TEST_F(CompactionTest, CompactTwiceIsSafe) {
    write_one_flush("a", "first");
    write_one_flush("b", "second");

    ASSERT_TRUE(db->compact());
    ASSERT_TRUE(db->compact());

    EXPECT_EQ(count_sst_files(), 1);
    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_EQ(db->get(std::format("a{:02d}", i)).value_or(""), "first");
    }
}

// 5. Space reclaim.
TEST_F(CompactionTest, ReclaimsSpaceFromOverwrites) {
    const std::string value(200, 'x');

    // Rewrite the same five keys twenty times. Every flush writes a fresh copy of
    // all five, so nineteen rounds worth of records become garbage.
    for (int round = 0; round < 20; ++round) {
        for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
            db->put(std::format("key{:02d}", i), value + std::to_string(round));
        }
    }

    ASSERT_GT(count_sst_files(), 10);
    uintmax_t bytes_before = total_sst_bytes();

    ASSERT_TRUE(db->compact());

    uintmax_t bytes_after = total_sst_bytes();
    EXPECT_EQ(count_sst_files(), 1);
    EXPECT_LT(bytes_after, bytes_before / 5)
        << "before=" << bytes_before << " after=" << bytes_after;

    // Only the newest round survives.
    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_EQ(db->get(std::format("key{:02d}", i)).value_or(""), value + "19");
    }
}

// 6. Automatic compaction.
// The default is on, so files never accumulate unless a caller asks for that.
TEST_F(CompactionTest, DefaultCompactionTriggerIsTen) {
    db.reset();
    IztaDB defaults(wal_path, sst_path);
    EXPECT_EQ(defaults.get_compaction_trigger(), 10);
}

// Reaching the trigger folds every file into one without anyone calling compact().
TEST_F(CompactionTest, AutomaticCompactionFiresAtTrigger) {
    db = make_db(3);

    write_one_flush("a", "v");
    EXPECT_EQ(count_sst_files(), 1);

    write_one_flush("b", "v");
    EXPECT_EQ(count_sst_files(), 2);

    // The third flush brings the count up to the trigger, so compaction runs
    // and folds all three files into one.
    write_one_flush("c", "v");
    EXPECT_EQ(count_sst_files(), 1);

    for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
        EXPECT_EQ(db->get(std::format("a{:02d}", i)).value_or(""), "v");
        EXPECT_EQ(db->get(std::format("b{:02d}", i)).value_or(""), "v");
        EXPECT_EQ(db->get(std::format("c{:02d}", i)).value_or(""), "v");
    }
}

// A trigger of zero leaves files alone, which the scaling benchmarks depend on.
TEST_F(CompactionTest, AutomaticCompactionIsDisabledAtZero) {
    db = make_db(0);

    for (int round = 0; round < 5; ++round) {
        write_one_flush(std::format("r{}k", round), "v");
    }

    EXPECT_EQ(count_sst_files(), 5);
}

// The whole point of the trigger: the file count stays bounded no matter how
// long writing goes on.
TEST_F(CompactionTest, AutomaticCompactionKeepsFileCountBounded) {
    db = make_db(5);

    // Forty flushes worth of writes. Without a trigger this leaves forty files.
    for (int round = 0; round < 40; ++round) {
        write_one_flush(std::format("r{:02d}k", round), "v");
        EXPECT_LE(count_sst_files(), 5) << "file count grew after round " << round;
    }

    // Nothing was dropped along the way.
    for (int round = 0; round < 40; ++round) {
        for (uintmax_t i = 0; i < kMemtableThreshold; ++i) {
            EXPECT_EQ(db->get(std::format("r{:02d}k{:02d}", round, i)).value_or(""), "v");
        }
    }
}

// Deletes must survive repeated automatic compactions, not resurrect.
TEST_F(CompactionTest, AutomaticCompactionPreservesDataWithDeletes) {
    db = make_db(3);

    // Fixed seed so a failure is reproducible.
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> key_pick(0, 29);
    std::uniform_int_distribution<int> op_pick(0, 3);

    std::map<std::string, std::string> oracle;

    for (int i = 0; i < 400; ++i) {
        std::string key = std::format("key{:02d}", key_pick(rng));

        // One operation in four is a delete.
        if (op_pick(rng) == 0) {
            db->remove(key);
            oracle.erase(key);
        } else {
            std::string value = std::format("v{:04d}", i);
            db->put(key, value);
            oracle[key] = value;
        }
    }

    for (const auto& [key, value] : oracle) {
        auto result = db->get(key);
        ASSERT_TRUE(result.has_value()) << "lost key " << key;
        EXPECT_EQ(result.value(), value) << "wrong value for " << key;
    }

    for (int i = 0; i < 30; ++i) {
        std::string key = std::format("key{:02d}", i);
        if (oracle.count(key)) continue;
        EXPECT_FALSE(db->get(key).has_value()) << "resurrected key " << key;
    }
}

// 7. Random churn against a std::map oracle.
TEST_F(CompactionTest, MatchesMapOracleUnderRandomChurn) {
    // Fixed seed so a failure is reproducible.
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> key_pick(0, 49);
    std::uniform_int_distribution<int> op_pick(0, 3);

    // The oracle is what the database must agree with at the end.
    std::map<std::string, std::string> oracle;

    const int total_ops = 600;
    const int compact_at = 400;

    for (int i = 0; i < total_ops; ++i) {
        std::string key = std::format("key{:02d}", key_pick(rng));

        // One operation in four is a delete.
        if (op_pick(rng) == 0) {
            db->remove(key);
            oracle.erase(key);
        } else {
            std::string value = std::format("v{:04d}", i);
            db->put(key, value);
            oracle[key] = value;
        }

        // Compact partway through, so later writes land on top of merged data.
        if (i == compact_at) {
            ASSERT_TRUE(db->compact());
        }
    }

    ASSERT_TRUE(db->compact());

    // Every key that should exist does, with the right value.
    for (const auto& [key, value] : oracle) {
        auto result = db->get(key);
        ASSERT_TRUE(result.has_value()) << "lost key " << key;
        EXPECT_EQ(result.value(), value) << "wrong value for " << key;
    }

    // Every key that should not exist stays gone.
    for (int i = 0; i < 50; ++i) {
        std::string key = std::format("key{:02d}", i);
        if (oracle.count(key)) continue;
        EXPECT_FALSE(db->get(key).has_value()) << "resurrected key " << key;
    }
}
