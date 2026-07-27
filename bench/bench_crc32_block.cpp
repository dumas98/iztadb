//
// Created by Daniel Dumas on 25/07/26.
//

#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>
#include "sstable_utils.h"

static void BM_Crc32Block(benchmark::State& state) {
    // Buffer built once, it has the size of a block of 4K bytes.
    int64_t block_bytes = state.range(0);
    std::vector<uint8_t> block_buffer(block_bytes, 0xAB);

    // Calculate the Checksum.
    for (auto _ : state) {
        uint32_t checksum = iztadb::sstable::calculate_crc32(block_buffer);
        benchmark::DoNotOptimize(checksum);
    }

    // Reports bytes/sec and time
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * block_bytes);
}

// Test how long it takes from writing to SSTable to perform the CRC32 checksum.
BENCHMARK(BM_Crc32Block)
    // Block size.
    ->Arg(4096)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(5)
    ->ReportAggregatesOnly(true);
