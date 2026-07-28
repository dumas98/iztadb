//
// Created by Daniel Dumas on 26/07/26.
//

#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "iztadb.h"

class ValueSizeGetFixture : public benchmark::Fixture {
public:
    // Same as memtable_threshold, only one flush and one file.
    static constexpr int64_t kNumOps = 100;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/value_size_get/wal";
    std::string sst_path = "./bench_data/value_size_get/sst";
    std::string bench_dir = "./bench_data/value_size_get/";
    std::vector<std::string> keys;
    std::string value;

    // Before each.
    void SetUp(const benchmark::State& state) override {
        // Experiment with different value sizes: 8, 32, 64, 128, or 256.
        int64_t value_size = state.range(0);

        std::filesystem::remove_all(bench_dir);
        // Threshold forces one flush and block density will be related to size.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       /*memtable_threshold=*/kNumOps,
                                       /*max_wal_file_size=*/32 * 1024 * 1024,
                                       /*block_size=*/4096);

        // Variable value size.
        value = std::string(value_size, 'x');

        // Add keys in format key_000000000000, key_000000000001
        // Triggers the one flush on the last put.
        keys.clear();
        keys.reserve(kNumOps);
        for (int64_t i = 0; i < kNumOps; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)i);
            keys.emplace_back(buf);
            db->put(keys.back(), value);
        }

        // Run once to "warm-up" so when the actual benchmark is running
        // it simulates production.
        for (const auto& key : keys) {
            auto warmup = db->get(key);
            benchmark::DoNotOptimize(warmup);
        }
    }

    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all(bench_dir);
    }
};

// Modify the value size per test to check different sizes.
BENCHMARK_DEFINE_F(ValueSizeGetFixture, GetVsValueSize)(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        std::optional<std::string> result = db->get(keys[i % kNumOps]);
        benchmark::DoNotOptimize(result);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(ValueSizeGetFixture, GetVsValueSize)
    ->RangeMultiplier(2)
    ->Range(8, 256)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);
