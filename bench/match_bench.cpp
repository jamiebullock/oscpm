/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "benchmarks.h"
#include "workload.h"

#include <oscpm/oscpm.h>

#include <string>
#include <string_view>

namespace oscpm_bench
{

namespace
{

    void matchPrepared(benchmark::State& state, const MatchPair* pair)
    {
        const oscpm::ParseResult parsed = oscpm::Pattern::parse(pair->pattern);
        if (!parsed || parsed.pattern().matches(pair->address) != pair->matches)
        {
            failBenchmark(state, "unexpected parse or match result");
            return;
        }
        std::string_view address = pair->address;
        for (auto _ : state)
        {
            oscpm::Pattern pattern = parsed.pattern();
            benchmark::DoNotOptimize(pattern);
            benchmark::DoNotOptimize(address);
            benchmark::DoNotOptimize(pattern.matches(address));
        }
    }

    void matchWithParse(benchmark::State& state, const MatchPair* pair)
    {
        if (oscpm::match(pair->pattern, pair->address) != pair->matches)
        {
            failBenchmark(state, "unexpected match result");
            return;
        }
        std::string_view pattern = pair->pattern;
        std::string_view address = pair->address;
        for (auto _ : state)
        {
            benchmark::DoNotOptimize(pattern);
            benchmark::DoNotOptimize(address);
            benchmark::DoNotOptimize(oscpm::match(pattern, address));
        }
    }

    void validateAddresses(benchmark::State& state, const Space* space)
    {
        for (const std::string& address : space->addresses)
        {
            if (oscpm::validateAddress(address))
            {
                failBenchmark(state, "malformed address " + address);
                return;
            }
        }
        for (auto _ : state)
        {
            for (const std::string& address : space->addresses)
            {
                benchmark::DoNotOptimize(oscpm::validateAddress(address));
            }
        }
    }

}

void registerMatchBenchmarks()
{
    for (const MatchPair& pair : matchPairs())
    {
        benchmark::RegisterBenchmark(std::string("Match/prepared/") + pair.id, matchPrepared, &pair);
        benchmark::RegisterBenchmark(std::string("Match/parse/") + pair.id, matchWithParse, &pair);
    }
    for (const Space& space : spaces())
    {
        benchmark::RegisterBenchmark(std::string("Validate/address/") + space.name, validateAddresses, &space);
    }
}

}
