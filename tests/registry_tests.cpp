/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm_regex/oscpm_regex.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <new>
#include <optional>
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

using oscpm_regex::AddressSpace;
using oscpm_regex::Error;

std::vector<std::string> visited(AddressSpace<int>& registry, std::string_view pattern, std::size_t& matched, std::optional<Error>& error)
{
    std::vector<std::string> addresses;
    const auto result = registry.dispatch(pattern, [&](std::string_view address, int&)
        { addresses.emplace_back(address); });
    matched = result.matched;
    error = result.error;
    return addresses;
}

AddressSpace<int> synth()
{
    AddressSpace<int> registry;
    registry.add("/synth/1/freq", 1);
    registry.add("/synth/1/amp", 2);
    registry.add("/synth/2/freq", 3);
    registry.add("/mixer/master/gain", 4);
    return registry;
}

}

TEST_CASE("add registers a well-formed address once")
{
    AddressSpace<int> registry;
    CHECK(registry.add("/a", 1) == std::nullopt);
    CHECK(registry.add("/a", 2) == Error::Duplicate);
    CHECK(registry.add("a", 3) == Error::MissingLeadingSlash);
    CHECK(registry.add("/a/", 3) == Error::TrailingSlash);
    CHECK(registry.add("/a//b", 3) == Error::EmptyPart);
    CHECK(registry.add("/a b", 3) == Error::IllegalByte);
    CHECK(registry.size() == 1);
}

TEST_CASE("remove unregisters an address that is registered")
{
    AddressSpace<int> registry = synth();
    CHECK(registry.remove("/synth/1/amp") == std::nullopt);
    CHECK(registry.remove("/synth/1/amp") == Error::NotFound);
    CHECK(registry.remove("synth") == Error::MissingLeadingSlash);
    CHECK(registry.size() == 3);
    std::size_t matched = 0;
    std::optional<Error> error;
    CHECK(visited(registry, "/synth/1/amp", matched, error).empty());
    CHECK(visited(registry, "/synth/1/freq", matched, error) == std::vector<std::string> { "/synth/1/freq" });
}

TEST_CASE("methods are visited in bytewise address order whatever the insertion order")
{
    AddressSpace<int> registry;
    registry.add("/c", 1);
    registry.add("/a", 2);
    registry.add("/b", 3);
    std::size_t matched = 0;
    std::optional<Error> error;
    CHECK(visited(registry, "/?", matched, error) == std::vector<std::string> { "/a", "/b", "/c" });
    registry.remove("/b");
    CHECK(visited(registry, "/?", matched, error) == std::vector<std::string> { "/a", "/c" });
}

TEST_CASE("an exact address reaches exactly its method")
{
    AddressSpace<int> registry = synth();
    std::size_t matched = 0;
    std::optional<Error> error;
    CHECK(visited(registry, "/synth/2/freq", matched, error) == std::vector<std::string> { "/synth/2/freq" });
    CHECK(matched == 1);
    CHECK_FALSE(error);
    CHECK(visited(registry, "/synth/3/freq", matched, error).empty());
    CHECK(matched == 0);
}

TEST_CASE("a wildcard pattern reaches every matching method")
{
    AddressSpace<int> registry = synth();
    std::size_t matched = 0;
    std::optional<Error> error;
    const auto addresses = visited(registry, "/synth/*/freq", matched, error);
    CHECK(matched == 2);
    CHECK(addresses == std::vector<std::string> { "/synth/1/freq", "/synth/2/freq" });
    CHECK(visited(registry, "//gain", matched, error) == std::vector<std::string> { "/mixer/master/gain" });
}

TEST_CASE("a malformed pattern reaches nothing and says so")
{
    AddressSpace<int> registry = synth();
    std::size_t matched = 0;
    std::optional<Error> error;
    CHECK(visited(registry, "/synth/[1/freq", matched, error).empty());
    CHECK(matched == 0);
    CHECK(error == Error::UnterminatedClass);
}

TEST_CASE("a repeated pattern is served from the cache and a change is seen at once")
{
    AddressSpace<int> registry = synth();
    std::size_t matched = 0;
    std::optional<Error> error;
    visited(registry, "/synth/*/freq", matched, error);
    CHECK(matched == 2);
    registry.add("/synth/3/freq", 5);
    visited(registry, "/synth/*/freq", matched, error);
    CHECK(matched == 3);
    registry.remove("/synth/1/freq");
    visited(registry, "/synth/*/freq", matched, error);
    CHECK(matched == 2);
}

TEST_CASE("a dispatch whose every pair is memoised allocates nothing")
{
    AddressSpace<int> registry = synth();
    const auto noop = [](std::string_view, int&) { };
    registry.dispatch("/synth/*/freq", noop);
    registry.dispatch("/synth/1/freq", noop);
    const std::size_t before = g_allocations;
    registry.dispatch("/synth/1/freq", noop);
    registry.dispatch("/synth/*/freq", noop);
    CHECK(g_allocations == before);
}

TEST_CASE("a memoised verdict allocates nothing")
{
    oscpm_regex::Matcher matcher;
    matcher.match("/synth/*/freq", "/synth/1/freq");
    const std::size_t before = g_allocations;
    CHECK(matcher.match("/synth/*/freq", "/synth/1/freq"));
    CHECK(g_allocations == before);
}

TEST_CASE("the visitor can change the value")
{
    AddressSpace<int> registry = synth();
    registry.dispatch("/synth/1/freq", [](std::string_view, int& value)
        { value = 440; });
    int seen = 0;
    registry.dispatch("/synth/1/freq", [&](std::string_view, int& value)
        { seen = value; });
    CHECK(seen == 440);
}
