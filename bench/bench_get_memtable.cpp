//
// Created by Daniel Dumas on 25/07/26.
//

#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "iztadb.h"

class MemTableGetFixture : public benchmark::Fixture {
public:
    // Keys generated into the keys vector.
    static constexpr int64_t kNumOps = 5000;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/get_memtable/wal";
    std::string sst_path = "./bench_data/get_memtable/sst";
    std::vector<std::string> keys;
    std::string value;

    // Before Each.
    void SetUp(const benchmark::State&) override {
        std::filesystem::remove_all("./bench_data/get_memtable/");
        // Set up high MemTable threshold.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       1'000'000,
                                       32 * 1024 * 1024,
                                       4096);
        // All values are x*100
        value = std::string(100, 'x');

        // Add keys in format key_000000000000, key_000000000001
        keys.clear();
        keys.reserve(kNumOps);
        for (int64_t i = 0; i < kNumOps; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)i);
            keys.emplace_back(buf);
        }

        // Populate the MemTable before running benchmark.
        for (const auto& key : keys) {
            db->put(key, value);
        }
    }

    // After Each.
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all("./bench_data/get_memtable/");
    }
};

// Execute a get operation to MemTable.
BENCHMARK_DEFINE_F(MemTableGetFixture, HitInMemTable)(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        std::optional<std::string> result = db->get(keys[i]);
        benchmark::DoNotOptimize(result);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}

// Repeat 500 times per repetition, use CPU time instead of Wall Time.
BENCHMARK_REGISTER_F(MemTableGetFixture, HitInMemTable)
    ->Iterations(MemTableGetFixture::kNumOps)
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);
