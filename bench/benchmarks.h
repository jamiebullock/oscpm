/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <benchmark/benchmark.h>

#include <string>

namespace oscpm_bench
{

/// Registers the `Match/` and `Validate/` benchmarks.
void registerMatchBenchmarks();

/// Registers the `Dispatch/` and `Build/` benchmarks.
void registerDispatchBenchmarks();

/// Skips `state` with `message` and makes the process exit non-zero, for a
/// benchmark whose workload did not produce its expected result.
void failBenchmark(benchmark::State& state, const std::string& message);

/// Whether any benchmark has called `failBenchmark`.
bool anyBenchmarkFailed();

}
