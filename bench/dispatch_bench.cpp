/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "benchmarks.h"
#include "workload.h"

#include <oscpm/address_space.h>
#include <oscpm/oscpm.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oscpm_bench
{

namespace
{

    using MemoisedSpace = oscpm::AddressSpace<int>;
    using UnmemoisedSpace = oscpm::AddressSpace<int, false>;

    constexpr std::size_t kModestAdversarialLength = 256;

    template <typename AddressSpace>
    void fill(AddressSpace& addressSpace, const Space& space)
    {
        for (const std::string& address : space.addresses)
        {
            addressSpace.add(address, 0);
        }
    }

    template <typename AddressSpace>
    oscpm::DispatchResult dispatchOnce(AddressSpace& addressSpace, std::string_view pattern)
    {
        benchmark::DoNotOptimize(pattern);
        return addressSpace.dispatch(pattern, [](std::string_view, int& value)
            { benchmark::DoNotOptimize(value); });
    }

    bool expectMatches(benchmark::State& state, const oscpm::DispatchResult& result, std::size_t numExpected)
    {
        if (!result.error && result.matched == numExpected)
        {
            return true;
        }
        failBenchmark(state, "visited " + std::to_string(result.matched) + ", expected " + std::to_string(numExpected) + (result.error ? ", pattern malformed" : ""));
        return false;
    }

    template <typename AddressSpace>
    void dispatchRepeatedly(benchmark::State& state, const Space* space, std::string pattern, std::size_t numExpected)
    {
        AddressSpace addressSpace;
        fill(addressSpace, *space);
        if (!expectMatches(state, dispatchOnce(addressSpace, pattern), numExpected))
        {
            return;
        }
        for (auto _ : state)
        {
            benchmark::DoNotOptimize(dispatchOnce(addressSpace, pattern));
        }
    }

    void dispatchStream(benchmark::State& state, const Space* space)
    {
        MemoisedSpace addressSpace;
        fill(addressSpace, *space);
        const std::vector<std::string> messages = messageStream(*space);
        oscpm::DispatchResult total { 0, std::nullopt };
        for (const std::string& message : messages)
        {
            const oscpm::DispatchResult result = dispatchOnce(addressSpace, message);
            total.matched += result.matched;
            total.error = total.error ? total.error : result.error;
        }
        if (!expectMatches(state, total, numStreamMatches(*space)))
        {
            return;
        }
        for (auto _ : state)
        {
            for (const std::string& message : messages)
            {
                benchmark::DoNotOptimize(dispatchOnce(addressSpace, message));
            }
        }
        state.counters["messages"] = static_cast<double>(messages.size());
    }

    void dispatchRejected(benchmark::State& state, const Space* space, std::string pattern)
    {
        MemoisedSpace addressSpace;
        fill(addressSpace, *space);
        const oscpm::DispatchResult result = dispatchOnce(addressSpace, pattern);
        if (!result.error || result.error->kind != oscpm::Error::PatternTooLong)
        {
            failBenchmark(state, "pattern not rejected as too long");
            return;
        }
        for (auto _ : state)
        {
            benchmark::DoNotOptimize(dispatchOnce(addressSpace, pattern));
        }
    }

    void build(benchmark::State& state, const Space* space)
    {
        for (auto _ : state)
        {
            MemoisedSpace addressSpace;
            fill(addressSpace, *space);
            if (addressSpace.size() != space->addresses.size())
            {
                failBenchmark(state, "an address was not added");
                return;
            }
            benchmark::DoNotOptimize(addressSpace);
        }
    }

    std::string nameFor(const char* group, const std::string& id, const Space& space)
    {
        return std::string("Dispatch/") + group + "/" + id + "/" + space.name;
    }

    void registerFor(const Space& space)
    {
        for (const MusicalPattern& pattern : musicalPatterns())
        {
            const std::size_t numExpected = pattern.numMatches[space.index];
            benchmark::RegisterBenchmark(nameFor("repeat", pattern.id, space), dispatchRepeatedly<MemoisedSpace>, &space, std::string(pattern.text), numExpected);
            benchmark::RegisterBenchmark(nameFor("cold", pattern.id, space), dispatchRepeatedly<UnmemoisedSpace>, &space, std::string(pattern.text), numExpected);
        }
        benchmark::RegisterBenchmark(std::string("Dispatch/stream/") + space.name, dispatchStream, &space);
        for (const std::size_t length : { kModestAdversarialLength, oscpm::kMaxPatternLength })
        {
            for (const AdversarialShape& shape : adversarialShapes())
            {
                const std::string id = std::string(shape.id) + "-" + std::to_string(length);
                benchmark::RegisterBenchmark(nameFor("adversarial", id, space), dispatchRepeatedly<UnmemoisedSpace>, &space, adversarialPattern(shape, length), std::size_t { 0 });
            }
        }
        const std::string oversized = adversarialPattern(adversarialShapes().front(), kDatagramPatternLength);
        benchmark::RegisterBenchmark(std::string("Dispatch/rejected/") + space.name, dispatchRejected, &space, oversized);
        benchmark::RegisterBenchmark(std::string("Build/") + space.name, build, &space);
    }

}

void registerDispatchBenchmarks()
{
    for (const Space& space : spaces())
    {
        registerFor(space);
    }
}

}
