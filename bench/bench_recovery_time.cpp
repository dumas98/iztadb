//
// Created by Daniel Dumas on 25/07/26.
//

#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include "iztadb.h"

class RecoveryTimeFixture : public benchmark::Fixture {
protected:
    std::string wal_path = "./bench_data/recovery_time/wal";
    std::string sst_path = "./bench_data/recovery_time/sst";
    std::string bench_dir = "./bench_data/recovery_time/";

    // Since this benchmark tests WAL recovery performance, a lower WAL segment threshold
    // will trigger more segment divisions than the standard 32MB (~333 records/segment).
    static constexpr uintmax_t kMaxWalFileSize = 128 * 1024;

    // Before each.
    void SetUp(const benchmark::State& state) override {
        int64_t num_records = state.range(0);

        std::filesystem::remove_all(bench_dir);

        // Start the db that will write to WAL and will be deleted and rebooted.
        auto priming_db = std::make_unique<IztaDB>(wal_path, sst_path,
                                                     10'000'000,
                                                     kMaxWalFileSize,
                                                     4096);

        // All values are x*100.
        std::string value(100, 'x');

        // Add keys in format key_000000000000, key_000000000001
        char buf[32];
        for (int64_t i = 0; i < num_records; ++i) {
            std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)i);
            priming_db->put(std::string(buf), value);
        }

        // Delete DB to reset it.
        priming_db.reset();
    }

    // After each.
    void TearDown(const benchmark::State&) override {
        std::filesystem::remove_all(bench_dir);
    }
};

// Reboot an instance of IztaDB, test inserting an increasing number of records.
BENCHMARK_DEFINE_F(RecoveryTimeFixture, ColdStartVsWalSize)(benchmark::State& state) {
    for (auto _ : state) {
        auto db = std::make_unique<IztaDB>(wal_path, sst_path,
                                            10'000'000,
                                            RecoveryTimeFixture::kMaxWalFileSize,
                                            4096);
        benchmark::DoNotOptimize(db);

        // Clean up rebooted db and exclude from time.
        state.PauseTiming();
        db.reset();
        state.ResumeTiming();
    }
}

BENCHMARK_REGISTER_F(RecoveryTimeFixture, ColdStartVsWalSize)
    ->RangeMultiplier(4)
    // 2, 6, 24, 96 segments.
    ->Range(500, 32000)
    ->Iterations(1)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);