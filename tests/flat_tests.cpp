/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm_flat/oscpm_flat.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Case
{
    const char* pattern;
    const char* address;
    bool expected;
};

const Case kCases[] = {
    { "/a", "/a", true },
    { "/a", "/b", false },
    { "/a*b", "/aXbYb", true },
    { "/a*b", "/aXbY", false },
    { "/{a,ab}b", "/abb", true },
    { "/{a,ab}{b,bb}", "/abbb", true },
    { "/{a,ab}{b,bb}", "/abbbb", false },
    { "/x{a,}", "/x", true },
    { "/x{,a}", "/xa", true },
    { "/[--a]", "/.", true },
    { "/[a--]", "/a", false },
    { "/[z-a]", "/m", false },
    { "/[!a]", "/b", true },
    { "/[!a]", "/a", false },
    { "/?", "/ab", false },
    { "//gain", "/x/y/gain", true },
    { "//a/b", "/a/x/a/b", true },
    { "/a//b//c", "/a/x/c/y/b/c", true },
    { "/a//b//c", "/a/c/b", false },
    { "/synth/*/{freq,amp}", "/synth/12/amp", true },
    { "/{a,b}{c,d}", "/bd", true },
    { "/{a,b}{c,d}", "/bb", false },
    { "/{a,b}x{c,d}", "/axd", true },
    { "/*", "/", true },
    { "/a*", "/a/b", false },
    { "/ch[1-4]*", "/ch1freq", true },
    { "/?[!a]*{x,y}", "/zbqqx", true },
    { "/?[!a]*{x,y}", "/zaqqx", false },
};

std::vector<std::string> dispatched(oscpm_flat::Registry<int>& registry, std::string_view pattern)
{
    std::vector<std::string> found;
    registry.dispatch(pattern, [&](std::string_view address, int&)
        { found.emplace_back(address); });
    return found;
}

}

TEST_CASE("match agrees with the expected result for every case")
{
    for (const Case& c : kCases)
    {
        INFO(c.pattern << " against " << c.address);
        CHECK(oscpm_flat::match(c.pattern, c.address) == c.expected);
    }
}

TEST_CASE("a compiled pattern matches as the one-shot match does")
{
    for (const Case& c : kCases)
    {
        INFO(c.pattern << " against " << c.address);
        const oscpm_flat::Pattern pattern(c.pattern);
        REQUIRE(pattern.valid());
        CHECK(pattern.matchAddress(c.address, oscpm_flat::Segments(c.address)) == c.expected);
    }
}

TEST_CASE("a registry dispatches a pattern to every matching method, repeatedly")
{
    oscpm_flat::Registry<int> registry;
    CHECK(registry.add("/synth/1/freq", 1));
    CHECK(registry.add("/synth/2/freq", 2));
    CHECK(registry.add("/synth/2/amp", 3));
    CHECK_FALSE(registry.add("/synth/1/freq", 4));
    for (int round = 0; round < 3; ++round)
    {
        std::vector<std::string> found = dispatched(registry, "/synth/*/freq");
        std::sort(found.begin(), found.end());
        CHECK(found == std::vector<std::string> { "/synth/1/freq", "/synth/2/freq" });
        CHECK(dispatched(registry, "/synth/2/amp") == std::vector<std::string> { "/synth/2/amp" });
    }
}

TEST_CASE("a registry reports a malformed pattern and visits nothing")
{
    oscpm_flat::Registry<int> registry;
    registry.add("/a", 1);
    std::size_t visited = 0;
    const auto result = registry.dispatch("/[a", [&](std::string_view, int&)
        { ++visited; });
    CHECK(result.malformed);
    CHECK(visited == 0);
}
