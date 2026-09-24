/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "benchmarks.h"

namespace oscpm_bench
{

namespace
{

    bool& failed()
    {
        static bool anyFailed = false;
        return anyFailed;
    }

}

void failBenchmark(benchmark::State& state, const std::string& message)
{
    failed() = true;
    state.SkipWithError(message);
}

bool anyBenchmarkFailed()
{
    return failed();
}

}

int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }
    benchmark::AddCustomContext("compiler", OSCPM_BENCH_COMPILER);
    oscpm_bench::registerMatchBenchmarks();
    oscpm_bench::registerDispatchBenchmarks();
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return oscpm_bench::anyBenchmarkFailed() ? 1 : 0;
}
