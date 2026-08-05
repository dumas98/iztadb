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

class NotFoundScalingFixture : public benchmark::Fixture {
public:
    // Keys generated that will be written to each SSTable.
    static constexpr int64_t kKeysPerFile = 100;

protected:
    std::unique_ptr<IztaDB> db;
    std::string wal_path = "./bench_data/get_sstable_scaling/wal";
    std::string sst_path = "./bench_data/get_sstable_scaling/sst";
    std::string bench_dir = "./bench_data/get_sstable_scaling/";

    // never inserted anywhere, to force a full scan of all SSTables.
    std::string missing_key;
    std::string value;

    // Before Each.
    void SetUp(const benchmark::State& state) override {
        std::filesystem::remove_all(bench_dir);

        // What run, evaluates from 1 to 32, increments of 2: [1, 2, 4, 8, 16, 32]
        int64_t num_files = state.range(0);

        // Second argument is the automatic compaction trigger. Each file count is
        // run twice, once with compaction off to build the baseline curve, once
        // with it on to show what compaction collapses that curve to.
        int64_t compaction_trigger = state.range(1);

        // Set up MemTable threshold same as the number of keys per file
        // so when the SSTables are populated the lookup is exclusively
        // scanning SSTables and no MemTable.
        db = std::make_unique<IztaDB>(wal_path, sst_path,
                                       kKeysPerFile,
                                       32 * 1024 * 1024,
                                      4096,
                                      compaction_trigger);

        // All values are x*100
        value = std::string(100, 'x');

        // The number of SSTables depends on the run.
        // Build num_files distinct SSTables, kKeysPerFile disjoint keys apiece.
        // Each batch's last put crosses the threshold, forcing one flush.
        char buf[32];
        for (int64_t file = 0; file < num_files; ++file) {
            for (int64_t i = 0; i < kKeysPerFile; ++i) {
                int64_t global_i = file * kKeysPerFile + i;
                std::snprintf(buf, sizeof(buf), "key_%012lld", (long long)global_i);
                db->put(std::string(buf), value);
            }
        }

        // This key was never inserted use zzz so
        missing_key = "aaa_this_key_was_never_inserted";

        // Run once to "warm-up" so when the actual benchmark is running
        // it simulates production.
        auto warmup = db->get(missing_key);
        benchmark::DoNotOptimize(warmup);
    }

    // After Each
    void TearDown(const benchmark::State&) override {
        db.reset();
        std::filesystem::remove_all(bench_dir);
    }

    // How many SSTables the lookup actually has to walk. Reported so the effect
    // of compaction is visible in the output and not just inferred from timing.
    double count_sst_files() {
        double count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sst_path)) {
            if (entry.path().extension() == ".sst") ++count;
        }
        return count;
    }
};

// Execute a get operation directly to an increasing number of SSTables
// to measure performance degrading, it will never find it forcing a scan
// of all SSTables.
BENCHMARK_DEFINE_F(NotFoundScalingFixture, GetMissingKeyVsN)(benchmark::State& state) {
    for (auto _ : state) {
        std::optional<std::string> result = db->get(missing_key);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
    state.counters["sst_files"] = count_sst_files();
}

// Repeat for N = 1, 2, 4, 8, 16, 32, each one twice: trigger 0 (compaction off,
// the baseline) then trigger 10 (compaction on). ArgsProduct varies the second
// argument fastest, so the two runs for a given N land next to each other.
BENCHMARK_REGISTER_F(NotFoundScalingFixture, GetMissingKeyVsN)
    ->ArgsProduct({{1, 2, 4, 8, 16, 32}, {0, 10}})
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);
