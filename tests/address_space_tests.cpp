/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"
#include "corpus.h"

#include <oscpm/address_space.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using oscpm::AddressSpace;
using oscpm::Error;
using oscpm::Pattern;

namespace
{

using Addresses = std::vector<std::string>;

Pattern parsed(std::string_view text)
{
    const oscpm::ParseResult result = Pattern::parse(text);
    REQUIRE(result);
    return result.pattern();
}

template <typename Space>
Addresses lookupAddresses(const Space& space, std::string_view pattern)
{
    Addresses found;
    space.lookup(parsed(pattern), [&](std::string_view address, const auto&)
        { found.emplace_back(address); });
    return found;
}

template <typename Space>
Addresses dispatchAddresses(const Space& space, std::string_view pattern, oscpm::DispatchResult& result)
{
    Addresses found;
    result = space.dispatch(pattern, [&](std::string_view address, const auto&)
        { found.emplace_back(address); });
    return found;
}

template <typename Space>
Addresses allAddresses(const Space& space)
{
    Addresses found;
    space.forEach([&](std::string_view address, const auto&)
        { found.emplace_back(address); });
    return found;
}

template <typename Space>
void populate(Space& space, const Addresses& addresses)
{
    for (const std::string& address : addresses)
    {
        REQUIRE_FALSE(space.add(address, 0).has_value());
    }
}

struct MayThrowOnMove
{
    MayThrowOnMove(int initial)
        : value(initial)
    {
    }

    MayThrowOnMove(const MayThrowOnMove&) = default;
    MayThrowOnMove& operator=(const MayThrowOnMove&) = default;

    MayThrowOnMove(MayThrowOnMove&& other) noexcept(false)
        : value(other.value)
    {
    }

    MayThrowOnMove& operator=(MayThrowOnMove&& other) noexcept(false)
    {
        value = other.value;
        return *this;
    }

    int value;
};

Addresses expectedMatches(const std::set<std::string>& registered, std::string_view pattern)
{
    Addresses expected;
    const Pattern value = parsed(pattern);
    for (const std::string& address : registered)
    {
        if (value.matches(address))
        {
            expected.push_back(address);
        }
    }
    return expected;
}

}

TEST_CASE("add registers a well-formed address once")
{
    AddressSpace<int> space;
    CHECK_FALSE(space.add("/synth/1/freq", 1).has_value());
    CHECK(space.add("/synth/1/freq", 2) == Error::Duplicate);
    CHECK(space.add("synth", 3) == Error::MissingLeadingSlash);
    CHECK(space.add("/synth/", 4) == Error::TrailingSlash);
    CHECK(space.add("/synth//freq", 5) == Error::EmptyPart);
    CHECK(space.add("/synth/*", 6) == Error::IllegalByte);
    CHECK(space.size() == 1);
}

TEST_CASE("remove unregisters an address that is registered")
{
    AddressSpace<int> space;
    populate(space, { "/a", "/b" });
    CHECK(space.remove("/c") == Error::NotFound);
    CHECK(space.remove("c") == Error::MissingLeadingSlash);
    CHECK_FALSE(space.remove("/a").has_value());
    CHECK(space.remove("/a") == Error::NotFound);
    CHECK(allAddresses(space) == Addresses { "/b" });
}

TEST_CASE("forEach visits methods in bytewise address order whatever the insertion order")
{
    AddressSpace<int> space;
    populate(space, { "/b", "/a/b", "/a", "/B", "/a/a", "/~", "/a-" });
    CHECK(allAddresses(space) == Addresses { "/B", "/a", "/a-", "/a/a", "/a/b", "/b", "/~" });
}

TEST_CASE("a literal pattern finds exactly the method with its text")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq", "/synth/1/freqs", "/synth/1", "/synth/10/freq" });
    CHECK(lookupAddresses(space, "/synth/1/freq") == Addresses { "/synth/1/freq" });
    CHECK(lookupAddresses(space, "/synth/1/fre") == Addresses { });
    CHECK(lookupAddresses(space, "/synth/1/freq/") == Addresses { });
    CHECK(lookupAddresses(space, "/synth/1/freq]") == Addresses { });
    CHECK(lookupAddresses(space, "/") == Addresses { });
}

TEST_CASE("a wildcard pattern finds every matching method in address order")
{
    AddressSpace<int> space;
    populate(space, { "/synth/2/amp", "/synth/1/freq", "/synth/1/amp", "/mixer/1/amp", "/synth/12/amp" });
    CHECK(lookupAddresses(space, "/synth/*/amp") == Addresses { "/synth/1/amp", "/synth/12/amp", "/synth/2/amp" });
    CHECK(lookupAddresses(space, "/synth/?/{amp,freq}") == Addresses { "/synth/1/amp", "/synth/1/freq", "/synth/2/amp" });
    CHECK(lookupAddresses(space, "//amp") == Addresses { "/mixer/1/amp", "/synth/1/amp", "/synth/12/amp", "/synth/2/amp" });
    CHECK(lookupAddresses(space, "//gain") == Addresses { });
    CHECK(lookupAddresses(space, "//") == allAddresses(space));
}

TEST_CASE("lookup returns the number of methods visited and lets the visitor change the value")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    CHECK(space.lookup(parsed("/a/*"), [](std::string_view, int& value)
              { value += 10; })
        == 2);
    std::map<std::string, int> values;
    space.forEach([&](std::string_view address, const int& value)
        { values[std::string(address)] = value; });
    CHECK(values == std::map<std::string, int> { { "/a/1", 10 }, { "/a/2", 10 }, { "/b/1", 0 } });
}

TEST_CASE("dispatch parses the pattern and visits what lookup visits")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq", "/synth/1/amp", "/synth/2/freq", "/mixer/gain" });
    for (const char* pattern : { "/synth/1/freq", "/synth/*/freq", "//amp", "/synth/3/*" })
    {
        INFO("pattern " << pattern);
        oscpm::DispatchResult result { 0, std::nullopt };
        const Addresses dispatched = dispatchAddresses(space, pattern, result);
        const Addresses looked = lookupAddresses(space, pattern);
        CHECK(dispatched == looked);
        CHECK(result.matched == looked.size());
        CHECK_FALSE(result.error.has_value());
    }
}

TEST_CASE("dispatch reports a malformed pattern and visits nothing")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq" });
    const AddressSpace<int>& constSpace = space;
    const struct
    {
        const char* pattern;
        Error kind;
        std::size_t offset;
    } cases[] = {
        { "synth/1/freq", Error::MissingLeadingSlash, 0 },
        { "/synth/[1/freq", Error::UnterminatedClass, 7 },
        { "/synth/{1/freq", Error::UnterminatedBraces, 7 },
    };
    for (const auto& malformed : cases)
    {
        INFO("pattern " << malformed.pattern);
        std::size_t visits = 0;
        const oscpm::DispatchResult mutableResult = space.dispatch(malformed.pattern, [&](std::string_view, int&)
            { ++visits; });
        const oscpm::DispatchResult constResult = constSpace.dispatch(malformed.pattern, [&](std::string_view, const int&)
            { ++visits; });
        CHECK(visits == 0);
        for (const oscpm::DispatchResult& result : { mutableResult, constResult })
        {
            CHECK(result.matched == 0);
            REQUIRE(result.error.has_value());
            CHECK(std::string(oscpm::toString(result.error->kind)) == std::string(oscpm::toString(malformed.kind)));
            CHECK(result.error->offset == malformed.offset);
        }
    }
}

TEST_CASE("a pattern of up to 64 parts and one of more are both matched in full")
{
    const auto repeated = [](const std::string& part, int count)
    {
        std::string text;
        for (int i = 0; i < count; ++i)
        {
            text += part;
        }
        return text;
    };
    AddressSpace<int> space;
    const std::string deep = repeated("/a", 70);
    populate(space, { "/a", repeated("/a", 64), repeated("/a", 64) + "/b", deep, deep + "/b" });
    CHECK(lookupAddresses(space, repeated("/*", 64)) == Addresses { repeated("/a", 64) });
    CHECK(lookupAddresses(space, repeated("/*", 65)) == Addresses { repeated("/a", 64) + "/b" });
    CHECK(lookupAddresses(space, repeated("/*", 70)) == Addresses { deep });
    CHECK(lookupAddresses(space, repeated("/*", 70) + "/b") == Addresses { deep + "/b" });
    CHECK(lookupAddresses(space, repeated("/a", 69) + "//b") == Addresses { deep + "/b" });
    CHECK(lookupAddresses(space, "/a" + repeated("//a", 69)) == Addresses { deep });
}

TEST_CASE("a value type whose move may throw is matched exactly as an int is")
{
    AddressSpace<int> ints;
    AddressSpace<MayThrowOnMove> mayThrow;
    std::string deep;
    for (int i = 0; i < 70; ++i)
    {
        deep += "/a";
    }
    const Addresses addresses { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp", "/mixer/gain", deep, deep + "/b" };
    populate(ints, addresses);
    populate(mayThrow, addresses);
    REQUIRE_FALSE(ints.remove("/synth/1/freq").has_value());
    REQUIRE_FALSE(mayThrow.remove("/synth/1/freq").has_value());
    for (const std::string& pattern : { std::string("/synth/*/freq"), std::string("//gain"), std::string("/*/2/{amp,freq}"), std::string("/a//b"), deep + "/*" })
    {
        INFO("pattern " << pattern);
        CHECK(lookupAddresses(mayThrow, pattern) == lookupAddresses(ints, pattern));
    }
}

TEST_CASE("dispatch reports a pattern longer than the supported length and visits nothing")
{
    AddressSpace<int> space;
    populate(space, { "/a" });
    std::size_t visits = 0;
    const oscpm::DispatchResult result = space.dispatch("/*" + std::string(oscpm::kMaxPatternLength, 'a'), [&](std::string_view, int&)
        { ++visits; });
    CHECK(visits == 0);
    CHECK(result.matched == 0);
    REQUIRE(result.error.has_value());
    CHECK(std::string(oscpm::toString(result.error->kind)) == "PatternTooLong");
    CHECK(result.error->offset == oscpm::kMaxPatternLength);
}

TEST_CASE("dispatch through a const space passes a const value")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2" });
    const AddressSpace<int>& constSpace = space;
    int sum = 0;
    const oscpm::DispatchResult result = constSpace.dispatch("/a/*", [&](std::string_view, const int& value)
        { sum += value + 1; });
    CHECK(result.matched == 2);
    CHECK_FALSE(result.error.has_value());
    CHECK(sum == 2);
}

TEST_CASE("a repeated lookup gives the same result and a change to the space is seen at once")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2" });
    const Addresses first = lookupAddresses(space, "/a/*");
    CHECK(lookupAddresses(space, "/a/*") == first);
    CHECK(first == Addresses { "/a/1", "/a/2" });

    REQUIRE_FALSE(space.add("/a/0", 0).has_value());
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/0", "/a/1", "/a/2" });
    REQUIRE_FALSE(space.remove("/a/1").has_value());
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/0", "/a/2" });
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/0", "/a/2" });
}

TEST_CASE("a single-bucket memo serves alternating patterns correctly")
{
    AddressSpace<int, true, 0, 4> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    for (int round = 0; round < 3; ++round)
    {
        CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
        CHECK(lookupAddresses(space, "/b/*") == Addresses { "/b/1" });
        CHECK(lookupAddresses(space, "/*/1") == Addresses { "/a/1", "/b/1" });
    }
}

TEST_CASE("a visitor may dispatch into the same space while a memoised result is delivered")
{
    AddressSpace<int, true, 0, 4> space;
    populate(space, { "/a/1", "/a/2", "/a/3", "/b/1", "/b/2" });
    const auto nested = [&](std::string_view pattern)
    {
        Addresses outer;
        Addresses inner;
        oscpm::DispatchResult innerResult { };
        const oscpm::DispatchResult outerResult = space.dispatch("/a/*", [&](std::string_view address, int&)
            {
                if (outer.empty())
                {
                    inner = dispatchAddresses(space, pattern, innerResult);
                }
                outer.emplace_back(address); });
        CHECK(outerResult.matched == 3);
        CHECK(outer == Addresses { "/a/1", "/a/2", "/a/3" });
        return inner;
    };

    CHECK(nested("/b/*") == Addresses { "/b/1", "/b/2" });
    CHECK(nested("/b/*") == Addresses { "/b/1", "/b/2" });
    CHECK(nested("/c/1").empty());
    CHECK(nested("/c/1").empty());
    CHECK(nested("/a/*") == Addresses { "/a/1", "/a/2", "/a/3" });
    CHECK(lookupAddresses(space, "/b/*") == Addresses { "/b/1", "/b/2" });
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2", "/a/3" });
}

TEST_CASE("a visitor may look up another pattern while a memoised result is delivered")
{
    AddressSpace<int, true, 0, 4> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    REQUIRE(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
    Addresses outer;
    Addresses inner;
    const Pattern pattern = parsed("/a/*");
    const std::size_t matched = space.lookup(pattern, [&](std::string_view address, int&)
        {
            if (outer.empty())
            {
                inner = lookupAddresses(space, "/b/*");
            }
            outer.emplace_back(address); });
    CHECK(matched == 2);
    CHECK(outer == Addresses { "/a/1", "/a/2" });
    CHECK(inner == Addresses { "/b/1" });
}

TEST_CASE("a result larger than the inline limit and a pattern longer than the memo limit are still delivered in full")
{
    AddressSpace<int, true, 2, 2> space;
    populate(space, { "/a/1", "/a/2", "/a/3", "/a/4" });
    for (int round = 0; round < 2; ++round)
    {
        CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2", "/a/3", "/a/4" });
        CHECK(lookupAddresses(space, "/a/[12]") == Addresses { "/a/1", "/a/2" });
    }
    const std::string longPattern = "/a/*" + std::string(oscpm::kMaxMemoPatternLength, '*');
    for (int round = 0; round < 2; ++round)
    {
        CHECK(lookupAddresses(space, longPattern) == Addresses { "/a/1", "/a/2", "/a/3", "/a/4" });
    }
}

TEST_CASE("a default space serves a result of 1024 methods from its memo unchanged")
{
    AddressSpace<int> space;
    Addresses voices;
    for (int voice = 0; voice < 1024; ++voice)
    {
        voices.push_back("/voice/" + std::to_string(voice));
    }
    populate(space, voices);
    populate(space, { "/master/gain", "/master/pan" });
    std::sort(voices.begin(), voices.end());
    for (int round = 0; round < 3; ++round)
    {
        CHECK(lookupAddresses(space, "/voice/*") == voices);
        CHECK(lookupAddresses(space, "/master/*") == Addresses { "/master/gain", "/master/pan" });
    }
}

TEST_CASE("an address space without a memo behaves the same")
{
    AddressSpace<int, false> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(lookupAddresses(space, "/a/1") == Addresses { "/a/1" });
    REQUIRE_FALSE(space.add("/a/0", 0).has_value());
    CHECK(lookupAddresses(space, "/a/*") == Addresses { "/a/0", "/a/1", "/a/2" });
}

TEST_CASE("dispatch sees every add and remove after a pattern has been dispatched")
{
    AddressSpace<int> space;
    populate(space, { "/a" });
    oscpm::DispatchResult result { 0, std::nullopt };
    for (int round = 0; round < 2; ++round)
    {
        CHECK(dispatchAddresses(space, "/b", result) == Addresses { });
        CHECK(dispatchAddresses(space, "/*", result) == Addresses { "/a" });
    }
    REQUIRE_FALSE(space.add("/b", 0).has_value());
    CHECK(dispatchAddresses(space, "/b", result) == Addresses { "/b" });
    CHECK(dispatchAddresses(space, "/*", result) == Addresses { "/a", "/b" });
    REQUIRE_FALSE(space.remove("/a").has_value());
    CHECK(dispatchAddresses(space, "/a", result) == Addresses { });
    CHECK(dispatchAddresses(space, "/*", result) == Addresses { "/b" });
}

TEST_CASE("every registered address dispatches to itself alone through interleaved adds and removes")
{
    AddressSpace<int> space;
    std::set<std::string> registered;
    std::mt19937 generator(3);
    for (int step = 0; step < 4000; ++step)
    {
        const std::string address = "/m/" + std::to_string(generator() % 600);
        if (generator() % 3 != 0)
        {
            space.add(address, 0);
            registered.insert(address);
        }
        else
        {
            space.remove(address);
            registered.erase(address);
        }
        if (step % 50 != 0)
        {
            continue;
        }
        for (const std::string& expected : registered)
        {
            oscpm::DispatchResult result { };
            REQUIRE(dispatchAddresses(space, expected, result) == Addresses { expected });
        }
        oscpm::DispatchResult absent { };
        CHECK(dispatchAddresses(space, "/m/600", absent).empty());
    }
}

TEST_CASE("a value type whose move may throw is dispatched exactly as an int is")
{
    AddressSpace<int> ints;
    AddressSpace<MayThrowOnMove> mayThrow;
    const Addresses addresses { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp", "/mixer/gain" };
    populate(ints, addresses);
    populate(mayThrow, addresses);
    REQUIRE_FALSE(ints.remove("/synth/1/freq").has_value());
    REQUIRE_FALSE(mayThrow.remove("/synth/1/freq").has_value());
    oscpm::DispatchResult intsResult { 0, std::nullopt };
    oscpm::DispatchResult mayThrowResult { 0, std::nullopt };
    for (int round = 0; round < 2; ++round)
    {
        for (const char* pattern : { "/synth/2/amp", "/synth/1/freq", "/synth/*/freq", "//gain", "/synth/[" })
        {
            INFO("pattern " << pattern);
            CHECK(dispatchAddresses(mayThrow, pattern, mayThrowResult) == dispatchAddresses(ints, pattern, intsResult));
            CHECK(mayThrowResult.matched == intsResult.matched);
            CHECK(mayThrowResult.error.has_value() == intsResult.error.has_value());
        }
    }
}

TEST_CASE("a copied address space and a moved-from one dispatch correctly")
{
    AddressSpace<int> source;
    populate(source, { "/a/1", "/a/2" });
    oscpm::DispatchResult result { 0, std::nullopt };
    CHECK(dispatchAddresses(source, "/a/1", result) == Addresses { "/a/1" });
    CHECK(dispatchAddresses(source, "/a/*", result) == Addresses { "/a/1", "/a/2" });

    const AddressSpace<int> copy(source);
    REQUIRE_FALSE(source.remove("/a/1").has_value());
    CHECK(dispatchAddresses(copy, "/a/1", result) == Addresses { "/a/1" });
    CHECK(dispatchAddresses(copy, "/a/*", result) == Addresses { "/a/1", "/a/2" });
    CHECK(dispatchAddresses(source, "/a/1", result) == Addresses { });
    CHECK(dispatchAddresses(source, "/a/*", result) == Addresses { "/a/2" });

    const AddressSpace<int> moved(std::move(source));
    CHECK(dispatchAddresses(moved, "/a/2", result) == Addresses { "/a/2" });
    CHECK(dispatchAddresses(source, "/a/2", result) == Addresses { });
    CHECK(dispatchAddresses(source, "/a/*", result) == Addresses { });
}

TEST_CASE("a moved address space keeps its methods and the moved-from one stays usable")
{
    AddressSpace<int> source;
    populate(source, { "/a/1", "/a/2" });
    CHECK(lookupAddresses(source, "/a/*") == Addresses { "/a/1", "/a/2" });

    AddressSpace<int> constructed(std::move(source));
    CHECK(lookupAddresses(constructed, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(lookupAddresses(constructed, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(lookupAddresses(source, "/a/*") == Addresses { });
    REQUIRE_FALSE(source.add("/b", 0).has_value());
    CHECK(lookupAddresses(source, "/*") == Addresses { "/b" });
    CHECK(lookupAddresses(source, "/*") == Addresses { "/b" });

    AddressSpace<int> assigned;
    assigned = std::move(constructed);
    CHECK(lookupAddresses(assigned, "/a/*") == Addresses { "/a/1", "/a/2" });
    REQUIRE_FALSE(constructed.add("/c", 0).has_value());
    CHECK(lookupAddresses(constructed, "//") == Addresses { "/c" });
    CHECK(lookupAddresses(constructed, "//") == Addresses { "/c" });
}

TEST_CASE("a move-only value type is stored and reached through lookup")
{
    AddressSpace<std::unique_ptr<int>> space;
    REQUIRE_FALSE(space.add("/a", std::make_unique<int>(1)).has_value());
    REQUIRE_FALSE(space.add("/b", std::make_unique<int>(2)).has_value());
    space.lookup(parsed("/*"), [](std::string_view, std::unique_ptr<int>& value)
        { *value *= 10; });
    int sum = 0;
    space.forEach([&](std::string_view, const std::unique_ptr<int>& value)
        { sum += *value; });
    CHECK(sum == 30);
}

TEST_CASE("a space of handlers dispatches to every matching handler without allocating")
{
    using Handler = std::function<void(int)>;
    static_assert(std::is_nothrow_move_constructible_v<Handler> && std::is_nothrow_move_assignable_v<Handler>);
    AddressSpace<Handler> handlers;
    std::vector<std::pair<std::string_view, int>> calls;
    for (const char* address : { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp" })
    {
        REQUIRE_FALSE(handlers.add(address, [&calls, address](int argument)
                                  { calls.emplace_back(address, argument); })
                .has_value());
    }
    calls.reserve(8);
    const auto pass = [](std::string_view, Handler& handler)
    { handler(440); };
    const auto passConst = [](std::string_view, const Handler& handler)
    { handler(220); };

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::DispatchResult wildcard = handlers.dispatch("/synth/*/freq", pass);
    const oscpm::DispatchResult literal = handlers.dispatch("/synth/2/amp", pass);
    const oscpm::DispatchResult absent = handlers.dispatch("/synth/3/freq", pass);
    const oscpm::DispatchResult viaConst = std::as_const(handlers).dispatch("/synth/2/*", passConst);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(wildcard.matched == 2);
    CHECK(literal.matched == 1);
    CHECK(absent.matched == 0);
    CHECK(viaConst.matched == 2);
    const std::vector<std::pair<std::string_view, int>> expected {
        { "/synth/1/freq", 440 }, { "/synth/2/freq", 440 }, { "/synth/2/amp", 440 }, { "/synth/2/amp", 220 }, { "/synth/2/freq", 220 }
    };
    CHECK(calls == expected);
}

TEST_CASE("lookup and forEach allocate nothing")
{
    AddressSpace<int> space;
    Addresses addresses;
    for (int i = 0; i < 100; ++i)
    {
        addresses.push_back("/synth/" + std::to_string(i) + "/freq");
    }
    populate(space, addresses);
    const Pattern literal = parsed("/synth/42/freq");
    const Pattern wildcard = parsed("/synth/?/freq");
    const std::string longText = "/synth/*/freq" + std::string(oscpm::kMaxMemoPatternLength, '*');
    const Pattern unmemoised = parsed(longText);
    std::size_t visited = 0;
    const auto count = [&](std::string_view, const int&)
    { ++visited; };

    const std::size_t before = oscpm_test::allocationCount();
    space.lookup(literal, count);
    space.lookup(wildcard, count);
    space.lookup(wildcard, count);
    space.lookup(unmemoised, count);
    space.forEach(count);
    const oscpm::DispatchResult literalResult = space.dispatch("/synth/42/freq", count);
    const oscpm::DispatchResult wildcardResult = space.dispatch("/synth/?/freq", count);
    const oscpm::DispatchResult malformedResult = space.dispatch("/synth/[4/freq", count);
    const oscpm::DispatchResult absentResult = space.dispatch("/synth/42/gain", count);
    const oscpm::DispatchResult absentAgainResult = space.dispatch("/synth/42/gain", count);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(visited == 1 + 10 + 10 + 100 + 100 + 1 + 10);
    CHECK(literalResult.matched == 1);
    CHECK(wildcardResult.matched == 10);
    CHECK(malformedResult.matched == 0);
    CHECK(malformedResult.error.has_value());
    CHECK(absentResult.matched == 0);
    CHECK(absentAgainResult.matched == 0);
}

TEST_CASE("every well-formed corpus pattern is delivered exactly as matches says")
{
    AddressSpace<int> space;
    std::set<std::string> registered;
    std::vector<std::string> patterns;
    std::vector<oscpm_test::CorpusCase> malformed;
    for (const oscpm_test::CorpusCase& corpusCase : oscpm_test::loadCorpus(OSCPM_CORPUS_PATH))
    {
        if (!oscpm::validateAddress(corpusCase.address) && registered.insert(corpusCase.address).second)
        {
            REQUIRE_FALSE(space.add(corpusCase.address, 0).has_value());
        }
        if (!oscpm::validatePattern(corpusCase.pattern))
        {
            patterns.push_back(corpusCase.pattern);
        }
        else if (corpusCase.expectation == oscpm_test::Expectation::MalformedPattern)
        {
            malformed.push_back(corpusCase);
        }
    }
    REQUIRE(space.size() == registered.size());
    REQUIRE_FALSE(malformed.empty());
    for (const std::string& pattern : patterns)
    {
        INFO("pattern " << pattern);
        const Addresses expected = expectedMatches(registered, pattern);
        CHECK(lookupAddresses(space, pattern) == expected);
        oscpm::DispatchResult result { 0, std::nullopt };
        CHECK(dispatchAddresses(space, pattern, result) == expected);
        CHECK(result.matched == expected.size());
        CHECK_FALSE(result.error.has_value());
    }
    for (const oscpm_test::CorpusCase& corpusCase : malformed)
    {
        INFO("corpus line " << corpusCase.line << ": " << corpusCase.text);
        oscpm::DispatchResult result { 0, std::nullopt };
        CHECK(dispatchAddresses(space, corpusCase.pattern, result).empty());
        CHECK(result.matched == 0);
        REQUIRE(result.error.has_value());
        CHECK(std::string(oscpm::toString(result.error->kind)) == corpusCase.errorName);
        CHECK(result.error->offset == corpusCase.offset);
    }
}

TEST_CASE("random adds, removes and lookups agree with the standalone matcher")
{
    AddressSpace<int, true, 2, 4> space;
    std::set<std::string> model;
    std::mt19937 random(20260920);
    const char* const parts[] = { "a", "b", "ab", "1", "12" };
    const char* const patternParts[] = { "a", "b", "*", "?", "[ab]", "{a,b}", "a*", "*1", "?b" };
    const auto pick = [&](const auto& items)
    {
        return items[random() % (sizeof(items) / sizeof(items[0]))];
    };
    const auto randomAddress = [&]
    {
        std::string address;
        const std::size_t numParts = 1 + random() % 3;
        for (std::size_t i = 0; i < numParts; ++i)
        {
            address += "/";
            address += pick(parts);
        }
        return address;
    };
    const auto randomPattern = [&]
    {
        std::string pattern;
        const std::size_t numParts = 1 + random() % 3;
        for (std::size_t i = 0; i < numParts; ++i)
        {
            pattern += random() % 4 == 0 ? "//" : "/";
            pattern += pick(patternParts);
        }
        return pattern;
    };

    for (int step = 0; step < 3000; ++step)
    {
        const std::size_t operation = random() % 4;
        if (operation == 0)
        {
            const std::string address = randomAddress();
            const bool fresh = model.insert(address).second;
            CHECK(space.add(address, step).has_value() == !fresh);
        }
        else if (operation == 1)
        {
            const std::string address = randomAddress();
            const bool present = model.erase(address) > 0;
            CHECK(space.remove(address).has_value() == !present);
        }
        else
        {
            const std::string pattern = randomPattern();
            INFO("step " << step << " pattern " << pattern);
            CHECK(lookupAddresses(space, pattern) == expectedMatches(model, pattern));
        }
        CHECK(allAddresses(space) == Addresses(model.begin(), model.end()));
    }
}
