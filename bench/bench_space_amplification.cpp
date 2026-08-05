//
// Created by Daniel Dumas on 26/07/26.
//

#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <string>
#include "iztadb.h"

class SpaceAmplificationFixture : public benchmark::Fixture {
public:
    static constexpr int64_t kAnchorCount = 20;
    static constexpr int64_t kChurnPoolSize = 20;
    static constexpr uintmax_t kMemtableThreshold = 20;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/space_amplification/wal";
    std::string sst_path = "./bench_data/space_amplification/sst";
    std::string bench_dir = "./bench_data/space_amplification/";
    std::string value;
    int64_t anchor_live_bytes = 0;

    // Before each.
    void SetUp(const benchmark::State& state) override {
        // Number of put + delete pairs: 50, 100, 200, 400, or 800
        int64_t churn_cycles = state.range(0);

        // Second argument is the automatic compaction trigger. Each churn count is
        // run twice, once with compaction off to show how much garbage accumulates,
        // once with it on to show how much of that gets reclaimed.
        int64_t compaction_trigger = state.range(1);

        std::filesystem::remove_all(bench_dir);
        // Threshold two flushes per put + delete pairs to increase the number of files.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       kMemtableThreshold,
                                       32 * 1024 * 1024,
                                       4096,
                                       compaction_trigger);

        // Always insert the same value.
        value = std::string(100, 'x');

        // Anchor, data will always be "live" so once compaction is implemented sweeping
        // deleted values will decrement read time.
        anchor_live_bytes = 0;
        char buf[32];
        for (int64_t a = 0; a < kAnchorCount; ++a) {
            std::snprintf(buf, sizeof(buf), "anchor_%08lld", (long long)a);
            std::string key(buf);
            db->put(key, value);
            anchor_live_bytes += static_cast<int64_t>(key.size() + value.size());
        }

        // Churn cycle, immediately after inserting it remove it living
        // garbage inside the files, performance will degrade.
        for (int64_t i = 0; i < churn_cycles; ++i) {
            std::snprintf(buf, sizeof(buf), "churn_%08lld", (long long)(i % kChurnPoolSize));
            std::string key(buf);
            db->put(key, value);
            db->remove(key);
        }
    }

    // After each.
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all(bench_dir);
    }

    // Sum bytes across every SSTable file.
    int64_t measure_sstable_disk_bytes() const {
        int64_t total = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sst_path)) {
            if (entry.is_regular_file()) {
                total += static_cast<int64_t>(entry.file_size());
            }
        }
        return total;
    }

    // How many SSTables the garbage is spread across. Reported so the effect of
    // compaction is visible in the output and not just inferred from the ratio.
    double count_sst_files() const {
        double count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sst_path)) {
            if (entry.path().extension() == ".sst") ++count;
        }
        return count;
    }
};

// Test won't use time, it will measure how much disk space increments as garbage
// starts accumulating.
BENCHMARK_DEFINE_F(SpaceAmplificationFixture, DiskVsLiveBytes)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        int64_t disk_bytes = measure_sstable_disk_bytes();
        double ratio;
        if (anchor_live_bytes > 0) {
            ratio = static_cast<double>(disk_bytes) / static_cast<double>(anchor_live_bytes);
        } else {
            ratio = 0.0;
        }

        // Focus on measuring bytes on disk, vs live bytes with its ratio.
        state.counters["disk_bytes"] = static_cast<double>(disk_bytes);
        state.counters["live_bytes"] = static_cast<double>(anchor_live_bytes);
        state.counters["amp_ratio"]  = ratio;
        state.counters["sst_files"]  = count_sst_files();
        state.ResumeTiming();
    }
}

// churn_cycles, the same points the old Range(50, 800) with multiplier 2 produced.
// Each one runs twice: trigger 0 (compaction off, the baseline) then trigger 10
// (compaction on). ArgsProduct varies the second argument fastest, so the two runs
// for a given churn count land next to each other.
BENCHMARK_REGISTER_F(SpaceAmplificationFixture, DiskVsLiveBytes)
    ->ArgsProduct({{50, 64, 128, 256, 512, 800}, {0, 10}})
    ->Iterations(1)
    ->UseRealTime();
