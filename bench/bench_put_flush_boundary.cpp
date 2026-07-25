//
// Created by Daniel Dumas on 24/07/26.
//

#include <benchmark/benchmark.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "iztadb.h"

class FlushBoundaryFixture : public benchmark::Fixture {
public:
    // Do more trials so there's chance the P999 will appear.
    // Each Trial will trigger one MemTable rotation.
    static constexpr int64_t kNumOps = 150;
    static constexpr int kNumTrials = 50;
    static constexpr uintmax_t kMemtableThreshold = 100;

protected:
    std::string wal_path = "./bench_data/put_flush_boundary/wal";
    std::string sst_path = "./bench_data/put_flush_boundary/sst";
    std::string bench_dir = "./bench_data/put_flush_boundary/";
    std::vector<std::string> keys;
    std::string value;

    // Before Each.
    void SetUp(const benchmark::State&) override {
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
        std::filesystem::remove_all(bench_dir);
    }
};

// Calculates benchmark for Flush, in CPU time.
BENCHMARK_DEFINE_F(FlushBoundaryFixture, PutAcrossFlush)(benchmark::State& state) {
    // Only run once, using State to register benchmarks.
    for (auto _ : state) {

        // Build vectors to store timings and exclude their construction from total time.
        // Two vectors, one for the flush latencies.
        state.PauseTiming();
        std::vector<double> latencies_us;
        std::vector<double> flush_put_latencies_us;
        std::vector<double> non_flush_latencies_us;
        latencies_us.reserve(kNumTrials * kNumOps);
        flush_put_latencies_us.reserve(kNumTrials);
        non_flush_latencies_us.reserve(kNumTrials * (kNumOps - 1));
        state.ResumeTiming();

        // Measure puts, restart db each trial so MemTable is only flushed once.
        for (int trial = 0; trial < kNumTrials; ++trial) {
            // Wipe previous trial's files and build a fresh IztaDB, excluded from timing.
            state.PauseTiming();
            std::filesystem::remove_all(bench_dir);
            auto db = std::make_unique<IztaDB>(wal_path, sst_path,
                                                kMemtableThreshold,
                                                32 * 1024 * 1024,
                                                4096);
            state.ResumeTiming();

            // Time each individual put by hand.
            for (int64_t i = 0; i < kNumOps; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                db->put(keys[i], value);
                auto end = std::chrono::high_resolution_clock::now();

                // Convert elapsed time to microseconds and store it.
                double us = std::chrono::duration<double, std::micro>(end - start).count();
                latencies_us.push_back(us);

                // Verify if it's the flush MemTable operation.
                if (i == static_cast<int64_t>(kMemtableThreshold) - 1) {
                    flush_put_latencies_us.push_back(us);
                } else {
                    non_flush_latencies_us.push_back(us);
                }
            }

            // Close this trial's IztaDB before the next trial wipes its files.
            state.PauseTiming();
            db.reset();
            state.ResumeTiming();
        }

        // Sort all three vectors so percentiles can be read directly off each array by position.
        state.PauseTiming();
        std::sort(latencies_us.begin(), latencies_us.end());
        std::sort(flush_put_latencies_us.begin(), flush_put_latencies_us.end());
        std::sort(non_flush_latencies_us.begin(), non_flush_latencies_us.end());

        // Lambda function to calculate percentiles.
        auto percentile = [](const std::vector<double>& sorted, double p) -> double {
            size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
            return sorted[idx];
        };

        // Small helper for plain averages, used on the flush/non-flush splits below.
        auto mean = [](const std::vector<double>& v) -> double {
            double sum = 0.0;
            for (double x : v) sum += x;
            return v.empty() ? 0.0 : sum / static_cast<double>(v.size());
        };

        // Record pooled percentiles across every put, all trials combined.
        state.counters["all_p50_us"]  = percentile(latencies_us, 0.50);
        state.counters["all_p99_us"]  = percentile(latencies_us, 0.99);
        state.counters["all_p999_us"] = percentile(latencies_us, 0.999);
        state.counters["all_max_us"]  = latencies_us.back();

        // The exact 100th put of each trial, isolated from everything else.
        state.counters["flush_put_mean_us"] = mean(flush_put_latencies_us);
        state.counters["flush_put_min_us"]  = flush_put_latencies_us.front();
        state.counters["flush_put_max_us"]  = flush_put_latencies_us.back();
        state.counters["flush_put_count"]   = static_cast<double>(flush_put_latencies_us.size());

        // The remaining 149-per-trial "normal" puts, for comparison.
        state.counters["non_flush_mean_us"] = mean(non_flush_latencies_us);
        state.counters["non_flush_p99_us"]  = percentile(non_flush_latencies_us, 0.99);

        state.ResumeTiming();
    }
}

BENCHMARK_REGISTER_F(FlushBoundaryFixture, PutAcrossFlush)
    ->Iterations(1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
