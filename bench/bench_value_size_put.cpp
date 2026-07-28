//
// Created by Daniel Dumas on 26/07/26.
//

#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "iztadb.h"

class ValueSizePutFixture : public benchmark::Fixture {
public:
    static constexpr int64_t kNumOps = 5000;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/value_size_put/wal";
    std::string sst_path = "./bench_data/value_size_put/sst";
    std::string bench_dir = "./bench_data/value_size_put/";
    std::vector<std::string> keys;
    std::string value;

    // Before each.
    void SetUp(const benchmark::State& state) override {
        // Experiment with different value sizes: 8, 32, 64, 128, or 256.
        int64_t value_size = state.range(0);

        std::filesystem::remove_all(bench_dir);
        // High MemTable threshold to isolate put time from flush.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       1'000'000,
                                       32 * 1024 * 1024,
                                       4096);

        // Variable value size.
        value = std::string(value_size, 'x');

        // Add keys in format key_000000000000, key_000000000001
        keys.clear();
        keys.reserve(kNumOps);
        for (int64_t i = 0; i < kNumOps; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)i);
            keys.emplace_back(buf);
        }
    }

    // After each.
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all(bench_dir);
    }
};

// Test one put with different value sizes.
BENCHMARK_DEFINE_F(ValueSizePutFixture, PutVsValueSize)(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        db->put(keys[i], value);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(ValueSizePutFixture, PutVsValueSize)
    ->RangeMultiplier(2)
    ->Range(8, 256)
    ->Iterations(ValueSizePutFixture::kNumOps)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);