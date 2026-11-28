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
#include <vector>

namespace oscpm_bench
{

namespace
{

    // In the GNU C library, `memchr` and `memcmp` take a number of instructions
    // that depends on where their argument starts, and a string literal moves
    // whenever the layout of the binary does.
    std::vector<char> onHeap(std::string_view text)
    {
        return std::vector<char>(text.begin(), text.end());
    }

    std::string_view viewOf(const std::vector<char>& bytes)
    {
        return std::string_view(bytes.data(), bytes.size());
    }

    void matchPrepared(benchmark::State& state, const MatchPair* pair)
    {
        const std::vector<char> patternBytes = onHeap(pair->pattern);
        const std::vector<char> addressBytes = onHeap(pair->address);
        const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> parsed = oscpm::Pattern::parse(viewOf(patternBytes));
        const oscpm::Expected<oscpm::Address, oscpm::AddressError> parsedAddress = oscpm::Address::parse(viewOf(addressBytes));
        if (!parsed || !parsedAddress || parsed->matches(*parsedAddress) != pair->matches)
        {
            failBenchmark(state, "unexpected parse or match result");
            return;
        }
        oscpm::Pattern pattern = *parsed;
        oscpm::Address address = *parsedAddress;
        for (auto _ : state)
        {
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
        const std::vector<char> patternBytes = onHeap(pair->pattern);
        const std::vector<char> addressBytes = onHeap(pair->address);
        std::string_view pattern = viewOf(patternBytes);
        std::string_view address = viewOf(addressBytes);
        for (auto _ : state)
        {
            benchmark::DoNotOptimize(pattern);
            benchmark::DoNotOptimize(address);
            benchmark::DoNotOptimize(oscpm::match(pattern, address));
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
}

}
