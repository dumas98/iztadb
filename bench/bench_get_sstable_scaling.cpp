//
// Created by Daniel Dumas on 25/07/26.
//

#include <benchmark/benchmark.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include "iztadb.h"

class SSTableScalingFixture : public benchmark::Fixture {
public:
    // Keys generated that will be written to each SSTable.
    static constexpr int64_t kKeysPerFile = 100;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/get_sstable_scaling/wal";
    std::string sst_path = "./bench_data/get_sstable_scaling/sst";
    std::string bench_dir = "./bench_data/get_sstable_scaling/";

    // Oldest key inserted to understand how scaling degrades performance.
    std::string target_key;
    std::string value;

    // Before Each.
    void SetUp(const benchmark::State& state) override {
        std::filesystem::remove_all(bench_dir);

        // What run, evaluates from 1 to 32, increments of 2: [1, 2, 4, 8, 16, 32]
        int64_t num_files = state.range(0);

        // Set up MemTable threshold same as the number of keys per file
        // so when the SSTables are populated the lookup is exclusively
        // scanning SSTables and no MemTable.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       kKeysPerFile,
                                       32 * 1024 * 1024,
                                      4096);

        // All values are x*100
        value = std::string(100, 'x');

        // The number of SSTable depends on the run.
        // Build num_files distinct SSTables, kKeysPerFile disjoint keys apiece.
        // Each batch's last put crosses the threshold, forcing exactly one flush.
        char buf[32];
        for (int64_t file = 0; file < num_files; ++file) {
            for (int64_t i = 0; i < kKeysPerFile; ++i) {
                int64_t global_i = file * kKeysPerFile + i;
                std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)global_i);
                std::string key(buf);
                db->put(key, value);
                // Save first key ever written. This is the worst case being measured.
                if (file == 0 && i == 0) {
                    target_key = key;
                }
            }
        }

        // Run once to "warm-up" so when the actual benchmark is running
        // it simulates production.
        auto warmup = db->get(target_key);
        benchmark::DoNotOptimize(warmup);
    }

    // After Each
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all(bench_dir);
    }
};

// Execute a get operation directly to an increasing number of SSTables
// to measure performance degrading.
BENCHMARK_DEFINE_F(SSTableScalingFixture, GetOldestKeyVsN)(benchmark::State& state) {
    for (auto _ : state) {
        std::optional<std::string> result = db->get(target_key);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}

// Repeat for N = 1, 2, 4, 8, 16, 32.
BENCHMARK_REGISTER_F(SSTableScalingFixture, GetOldestKeyVsN)
    ->RangeMultiplier(2)
    ->Range(1, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);
