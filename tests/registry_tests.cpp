/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <doctest/doctest.h>

#include <algorithm>
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace
{

using oscpm::AddressSpace;

std::vector<std::string> dispatched(AddressSpace<int>& space, std::string_view pattern, std::size_t& matched)
{
    std::vector<std::string> addresses;
    matched = space.dispatch(pattern, [&](std::string_view address, int&)
        { addresses.emplace_back(address); });
    return addresses;
}

AddressSpace<int> synth()
{
    AddressSpace<int> space;
    space.add("/synth/1/freq", 1);
    space.add("/synth/1/amp", 2);
    space.add("/synth/2/freq", 3);
    space.add("/mixer/master/gain", 4);
    return space;
}

}

TEST_CASE("add registers an address once")
{
    AddressSpace<int> space;
    CHECK(space.add("/a", 1));
    CHECK_FALSE(space.add("/a", 2));
    CHECK(space.size() == 1);
}

TEST_CASE("remove unregisters an address that is registered")
{
    AddressSpace<int> space = synth();
    CHECK(space.remove("/synth/1/amp"));
    CHECK_FALSE(space.remove("/synth/1/amp"));
    CHECK(space.size() == 3);
    std::size_t matched = 0;
    CHECK(dispatched(space, "/synth/1/amp", matched).empty());
    CHECK(dispatched(space, "/synth/1/freq", matched) == std::vector<std::string> { "/synth/1/freq" });
}

TEST_CASE("a wildcard visits every method once, in no particular order")
{
    AddressSpace<int> space;
    space.add("/c", 1);
    space.add("/a", 2);
    space.add("/b", 3);
    std::size_t matched = 0;
    std::vector<std::string> addresses = dispatched(space, "/?", matched);
    std::sort(addresses.begin(), addresses.end());
    CHECK(addresses == std::vector<std::string> { "/a", "/b", "/c" });
    CHECK(matched == 3);
}

TEST_CASE("an exact address reaches exactly its method")
{
    AddressSpace<int> space = synth();
    std::size_t matched = 0;
    CHECK(dispatched(space, "/synth/2/freq", matched) == std::vector<std::string> { "/synth/2/freq" });
    CHECK(matched == 1);
    CHECK(dispatched(space, "/synth/3/freq", matched).empty());
    CHECK(matched == 0);
}

TEST_CASE("a wildcard pattern reaches every matching method")
{
    AddressSpace<int> space = synth();
    std::size_t matched = 0;
    std::vector<std::string> addresses = dispatched(space, "/synth/*/freq", matched);
    std::sort(addresses.begin(), addresses.end());
    CHECK(addresses == std::vector<std::string> { "/synth/1/freq", "/synth/2/freq" });
    CHECK(matched == 2);
    CHECK(dispatched(space, "//gain", matched) == std::vector<std::string> { "/mixer/master/gain" });
}

TEST_CASE("an invalid pattern reaches nothing")
{
    AddressSpace<int> space = synth();
    std::size_t matched = 0;
    CHECK(dispatched(space, "/synth/[1/freq", matched).empty());
    CHECK(matched == 0);
}

TEST_CASE("a change to the space is seen at once")
{
    AddressSpace<int> space = synth();
    std::size_t matched = 0;
    dispatched(space, "/synth/*/freq", matched);
    CHECK(matched == 2);
    space.add("/synth/3/freq", 5);
    dispatched(space, "/synth/*/freq", matched);
    CHECK(matched == 3);
    space.remove("/synth/1/freq");
    dispatched(space, "/synth/*/freq", matched);
    CHECK(matched == 2);
}

TEST_CASE("a pattern equal to a registered address reaches it by one lookup, allocating nothing even the first time")
{
    AddressSpace<int> space = synth();
    const auto noop = [](std::string_view, int&) { };
    const std::size_t before = g_allocations;
    CHECK(space.dispatch("/synth/1/amp", noop) == 1);
    CHECK(g_allocations == before);
}

TEST_CASE("a pattern dispatched before allocates nothing")
{
    AddressSpace<int> space = synth();
    const auto noop = [](std::string_view, int&) { };
    space.dispatch("/synth/*/freq", noop);
    space.dispatch("/synth/1/freq", noop);
    const std::size_t before = g_allocations;
    space.dispatch("/synth/1/freq", noop);
    space.dispatch("/synth/*/freq", noop);
    CHECK(g_allocations == before);
}

TEST_CASE("a memoised verdict allocates nothing and an invalid pattern is memoised as matching nothing")
{
    oscpm::Matcher matcher;
    matcher.match("/synth/*/freq", "/synth/1/freq");
    matcher.match("/synth/[1", "/synth/1");
    const std::size_t before = g_allocations;
    CHECK(matcher.match("/synth/*/freq", "/synth/1/freq"));
    CHECK_FALSE(matcher.match("/synth/*/freq", "/synth/1/amp"));
    CHECK_FALSE(matcher.match("/synth/[1", "/synth/1"));
    CHECK(g_allocations > before);
    const std::size_t warm = g_allocations;
    CHECK(matcher.match("/synth/*/freq", "/synth/1/freq"));
    CHECK_FALSE(matcher.match("/synth/[1", "/synth/1"));
    CHECK(g_allocations == warm);
}

TEST_CASE("the memo is emptied when it reaches its limit")
{
    oscpm::Matcher matcher(2);
    matcher.match("/a", "/a");
    matcher.match("/b", "/b");
    std::size_t before = g_allocations;
    CHECK(matcher.match("/a", "/a"));
    CHECK(g_allocations == before);
    matcher.match("/c", "/c");
    before = g_allocations;
    CHECK(matcher.match("/a", "/a"));
    CHECK(g_allocations > before);
}

TEST_CASE("the callback can change the value")
{
    AddressSpace<int> space = synth();
    space.dispatch("/synth/1/freq", [](std::string_view, int& value)
        { value = 440; });
    int seen = 0;
    space.dispatch("/synth/1/freq", [&](std::string_view, int& value)
        { seen = value; });
    CHECK(seen == 440);
}
