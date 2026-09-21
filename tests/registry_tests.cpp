/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm_regex/oscpm_regex.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::size_t g_allocations = 0;

}

void* operator new(std::size_t size)
{
    ++g_allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{

using oscpm_regex::Registry;

std::vector<std::string> visited(Registry<int>& registry, std::string_view pattern, std::size_t& matched, bool& malformed)
{
    std::vector<std::string> addresses;
    const auto result = registry.dispatch(pattern, [&](std::string_view address, int&)
        { addresses.emplace_back(address); });
    matched = result.matched;
    malformed = result.malformed;
    return addresses;
}

Registry<int> synth()
{
    Registry<int> registry;
    registry.add("/synth/1/freq", 1);
    registry.add("/synth/1/amp", 2);
    registry.add("/synth/2/freq", 3);
    registry.add("/mixer/master/gain", 4);
    return registry;
}

}

TEST_CASE("add registers a well-formed address once")
{
    Registry<int> registry;
    CHECK(registry.add("/a", 1));
    CHECK_FALSE(registry.add("/a", 2));
    CHECK_FALSE(registry.add("a", 3));
    CHECK_FALSE(registry.add("/a/", 3));
    CHECK(registry.size() == 1);
}

TEST_CASE("remove unregisters an address that is registered")
{
    Registry<int> registry = synth();
    CHECK(registry.remove("/synth/1/amp"));
    CHECK_FALSE(registry.remove("/synth/1/amp"));
    CHECK(registry.size() == 3);
    std::size_t matched = 0;
    bool malformed = false;
    CHECK(visited(registry, "/synth/1/amp", matched, malformed).empty());
    CHECK(visited(registry, "/synth/1/freq", matched, malformed) == std::vector<std::string> { "/synth/1/freq" });
}

TEST_CASE("an exact address reaches exactly its method")
{
    Registry<int> registry = synth();
    std::size_t matched = 0;
    bool malformed = false;
    CHECK(visited(registry, "/synth/2/freq", matched, malformed) == std::vector<std::string> { "/synth/2/freq" });
    CHECK(matched == 1);
    CHECK_FALSE(malformed);
    CHECK(visited(registry, "/synth/3/freq", matched, malformed).empty());
    CHECK(matched == 0);
}

TEST_CASE("a wildcard pattern reaches every matching method")
{
    Registry<int> registry = synth();
    std::size_t matched = 0;
    bool malformed = false;
    const auto addresses = visited(registry, "/synth/*/freq", matched, malformed);
    CHECK(matched == 2);
    CHECK(addresses.size() == 2);
    CHECK(visited(registry, "//gain", matched, malformed) == std::vector<std::string> { "/mixer/master/gain" });
}

TEST_CASE("a malformed pattern reaches nothing and says so")
{
    Registry<int> registry = synth();
    std::size_t matched = 0;
    bool malformed = false;
    CHECK(visited(registry, "/synth/[1/freq", matched, malformed).empty());
    CHECK(matched == 0);
    CHECK(malformed);
}

TEST_CASE("a repeated pattern is served from the cache and a change is seen at once")
{
    Registry<int> registry = synth();
    std::size_t matched = 0;
    bool malformed = false;
    visited(registry, "/synth/*/freq", matched, malformed);
    CHECK(matched == 2);
    registry.add("/synth/3/freq", 5);
    visited(registry, "/synth/*/freq", matched, malformed);
    CHECK(matched == 3);
    registry.remove("/synth/1/freq");
    visited(registry, "/synth/*/freq", matched, malformed);
    CHECK(matched == 2);
}

TEST_CASE("the exact path and a cache hit allocate nothing")
{
    Registry<int> registry = synth();
    const auto noop = [](std::string_view, int&) { };
    registry.dispatch("/synth/*/freq", noop);
    const std::size_t before = g_allocations;
    registry.dispatch("/synth/1/freq", noop);
    registry.dispatch("/synth/*/freq", noop);
    CHECK(g_allocations == before);
}

TEST_CASE("the visitor can change the value")
{
    Registry<int> registry = synth();
    registry.dispatch("/synth/1/freq", [](std::string_view, int& value)
        { value = 440; });
    int seen = 0;
    registry.dispatch("/synth/1/freq", [&](std::string_view, int& value)
        { seen = value; });
    CHECK(seen == 440);
}
