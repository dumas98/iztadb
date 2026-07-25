//
// Created by Daniel Dumas on 24/07/26.
//

#include <benchmark/benchmark.h>
#include "iztadb.h"

class SequentialPutFixture : public benchmark::Fixture {
    protected:
        std::unique_ptr<IztaDB> db;
        std::string wal_path = "./bench_data/put_sequential/wal";
        std::string sst_path = "./bench_data/put_sequential/sst";
        std::vector<std::string> keys;
        std::string value;

    public:
        // Keys generated into the keys vector.
        static constexpr int64_t kNumOps = 5000;

    // Before Each.
    void SetUp(const benchmark::State&) override {
        std::filesystem::remove_all("./bench_data/");
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
    }

    // After Each.
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all("./bench_data/");
    }
};

// Execute a put operation.
BENCHMARK_DEFINE_F(SequentialPutFixture, ColdPut)(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        db->put(keys[i], value);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}

// Repeat 500 times per repetition.
BENCHMARK_REGISTER_F(SequentialPutFixture, ColdPut)
    ->Iterations(SequentialPutFixture::kNumOps)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);

BENCHMARK_MAIN();