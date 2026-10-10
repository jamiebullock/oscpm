/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"
#include "corpus.h"

#include <oscpm/address_space.h>
#include <oscpm/detail/address_index.h>
#include <oscpm/detail/dispatch_memo.h>

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

using oscpm::Address;
using oscpm::AddressSpace;
using oscpm::Pattern;
using oscpm::PatternError;

namespace
{

using Addresses = std::vector<std::string>;

bool faults(const std::optional<PatternError>& result, PatternError kind)
{
    return result == kind;
}

Pattern parsed(std::string_view text)
{
    const oscpm::Expected<Pattern, oscpm::PatternError> result = Pattern::parse(text);
    REQUIRE(result);
    return *result;
}

Address parsedAddress(std::string_view text)
{
    const oscpm::Expected<Address, oscpm::AddressError> result = Address::parse(text);
    REQUIRE(result);
    return *result;
}

template <typename Space>
Addresses visitAddresses(const Space& space, std::string_view pattern, oscpm::MatchResult& result)
{
    Addresses found;
    result = space.visit(pattern, [&](std::string_view address, const auto&)
        { found.emplace_back(address); });
    return found;
}

template <typename Space>
Addresses matchingAddresses(const Space& space, std::string_view pattern)
{
    oscpm::MatchResult result { 0, std::nullopt };
    Addresses found = visitAddresses(space, pattern, result);
    REQUIRE_FALSE(result.error.has_value());
    REQUIRE(result.matched == found.size());
    return found;
}

template <typename Space>
Addresses allAddresses(const Space& space)
{
    Addresses found;
    space.visit([&](std::string_view address, const auto&)
        { found.emplace_back(address); });
    return found;
}

template <typename Space>
void populate(Space& space, const Addresses& addresses)
{
    for (const std::string& address : addresses)
    {
        REQUIRE(space.add(parsedAddress(address), 0));
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

std::optional<std::size_t> indexedPosition(const oscpm::detail::AddressIndex& index, const Addresses& sorted, const std::string& address)
{
    std::size_t position = 0;
    const bool found = index.find(oscpm::detail::hashOf(address), [&](std::size_t candidate)
        { return sorted[candidate] == address; }, position);
    return found ? std::optional<std::size_t>(position) : std::nullopt;
}

Addresses expectedMatches(const std::set<std::string>& registered, std::string_view pattern)
{
    Addresses expected;
    const Pattern value = parsed(pattern);
    for (const std::string& address : registered)
    {
        if (value.matches(*Address::parse(address)))
        {
            expected.push_back(address);
        }
    }
    return expected;
}

}

TEST_CASE("add registers an address once")
{
    AddressSpace<int> space;
    CHECK(space.add(parsedAddress("/synth/1/freq"), 1));
    CHECK_FALSE(space.add(parsedAddress("/synth/1/freq"), 2));
    CHECK(space.size() == 1);
    CHECK(*space.find("/synth/1/freq") == 1);
}

TEST_CASE("remove unregisters an address that is registered")
{
    AddressSpace<int> space;
    populate(space, { "/a", "/b" });
    CHECK_FALSE(space.remove(parsedAddress("/c")));
    CHECK(space.remove(parsedAddress("/a")));
    CHECK_FALSE(space.remove(parsedAddress("/a")));
    CHECK(allAddresses(space) == Addresses { "/b" });
}

TEST_CASE("visit without a pattern reaches every address in bytewise address order whatever the insertion order")
{
    AddressSpace<int> space;
    populate(space, { "/b", "/a/b", "/a", "/B", "/a/a", "/~", "/a-" });
    CHECK(allAddresses(space) == Addresses { "/B", "/a", "/a-", "/a/a", "/a/b", "/b", "/~" });
}

TEST_CASE("a literal pattern finds exactly the address equal to its text")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq", "/synth/1/freqs", "/synth/1", "/synth/10/freq" });
    CHECK(matchingAddresses(space, "/synth/1/freq") == Addresses { "/synth/1/freq" });
    CHECK(matchingAddresses(space, "/synth/1/fre") == Addresses { });
    CHECK(matchingAddresses(space, "/synth/1/freq/") == Addresses { });
    CHECK(matchingAddresses(space, "/synth/1/freq]") == Addresses { });
    CHECK(matchingAddresses(space, "/") == Addresses { });
}

TEST_CASE("a wildcard pattern finds every matching address in address order")
{
    AddressSpace<int> space;
    populate(space, { "/synth/2/amp", "/synth/1/freq", "/synth/1/amp", "/mixer/1/amp", "/synth/12/amp" });
    CHECK(matchingAddresses(space, "/synth/*/amp") == Addresses { "/synth/1/amp", "/synth/12/amp", "/synth/2/amp" });
    CHECK(matchingAddresses(space, "/synth/?/{amp,freq}") == Addresses { "/synth/1/amp", "/synth/1/freq", "/synth/2/amp" });
    CHECK(matchingAddresses(space, "//amp") == Addresses { "/mixer/1/amp", "/synth/1/amp", "/synth/12/amp", "/synth/2/amp" });
    CHECK(matchingAddresses(space, "//gain") == Addresses { });
    CHECK(matchingAddresses(space, "//") == allAddresses(space));
}

TEST_CASE("visit returns the number of values visited and lets the visitor change the value")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    const oscpm::MatchResult result = space.visit("/a/*", [](std::string_view, int& value)
        { value += 10; });
    CHECK(result.matched == 2);
    CHECK_FALSE(result.error.has_value());
    std::map<std::string, int> values;
    space.visit([&](std::string_view address, const int& value)
        { values[std::string(address)] = value; });
    CHECK(values == std::map<std::string, int> { { "/a/1", 10 }, { "/a/2", 10 }, { "/b/1", 0 } });
}

TEST_CASE("find returns the value registered under an address and null when there is none")
{
    AddressSpace<int> space;
    REQUIRE(space.add(parsedAddress("/synth/1/freq"), 440));
    REQUIRE(space.add(parsedAddress("/synth/2/freq"), 220));

    int* const found = space.find("/synth/1/freq");
    REQUIRE(found != nullptr);
    CHECK(*found == 440);
    *found = 550;
    int visited = 0;
    space.visit("/synth/1/freq", [&](std::string_view, int& value)
        { visited = value; });
    CHECK(visited == 550);

    for (const char* absent : { "/synth/3/freq", "/synth/1", "/synth/1/freq/", "/synth/*/freq", "synth/1/freq", "/", "" })
    {
        INFO("address " << absent);
        CHECK(space.find(absent) == nullptr);
    }

    REQUIRE(space.remove(parsedAddress("/synth/1/freq")));
    CHECK(space.find("/synth/1/freq") == nullptr);
    REQUIRE(space.find("/synth/2/freq") != nullptr);
    CHECK(*space.find("/synth/2/freq") == 220);
}

TEST_CASE("find through a const space returns a pointer to a const value")
{
    AddressSpace<int> space;
    REQUIRE(space.add(parsedAddress("/a"), 7));
    const AddressSpace<int>& constSpace = space;
    static_assert(std::is_same_v<decltype(constSpace.find("/a")), const int*>);
    static_assert(std::is_same_v<decltype(space.find("/a")), int*>);
    REQUIRE(constSpace.find("/a") != nullptr);
    CHECK(*constSpace.find("/a") == 7);
    CHECK(constSpace.find("/b") == nullptr);
}

TEST_CASE("find reaches a value whose move may throw, a move-only value and a value in a space without a memo")
{
    AddressSpace<MayThrowOnMove> mayThrow;
    REQUIRE(mayThrow.add(parsedAddress("/a"), MayThrowOnMove(1)));
    REQUIRE(mayThrow.add(parsedAddress("/b"), MayThrowOnMove(2)));
    REQUIRE(mayThrow.find("/b") != nullptr);
    CHECK(mayThrow.find("/b")->value == 2);
    CHECK(mayThrow.find("/c") == nullptr);

    AddressSpace<std::unique_ptr<int>> moveOnly;
    REQUIRE(moveOnly.add(parsedAddress("/a"), std::make_unique<int>(3)));
    REQUIRE(moveOnly.find("/a") != nullptr);
    CHECK(**moveOnly.find("/a") == 3);

    AddressSpace<int, false> unmemoised;
    REQUIRE(unmemoised.add(parsedAddress("/a"), 4));
    REQUIRE(unmemoised.find("/a") != nullptr);
    CHECK(*unmemoised.find("/a") == 4);
    CHECK(unmemoised.find("/b") == nullptr);
}

TEST_CASE("a visitor may call find on the space that called it")
{
    AddressSpace<int> space;
    REQUIRE(space.add(parsedAddress("/a/1"), 1));
    REQUIRE(space.add(parsedAddress("/a/2"), 2));
    REQUIRE(space.add(parsedAddress("/total"), 0));
    const oscpm::MatchResult result = space.visit("/a/*", [&](std::string_view, int& value)
        { *space.find("/total") += value; });
    CHECK(result.matched == 2);
    CHECK(*space.find("/total") == 3);
}

TEST_CASE("visit reaches the addresses a pattern matches and none for a pattern that matches nothing")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq", "/synth/1/amp", "/synth/2/freq", "/mixer/gain" });
    const struct
    {
        const char* pattern;
        Addresses expected;
    } cases[] = {
        { "/synth/1/freq", { "/synth/1/freq" } },
        { "/synth/*/freq", { "/synth/1/freq", "/synth/2/freq" } },
        { "//amp", { "/synth/1/amp" } },
        { "/synth/3/*", { } },
    };
    for (const auto& expectation : cases)
    {
        INFO("pattern " << expectation.pattern);
        oscpm::MatchResult result { 0, std::nullopt };
        CHECK(visitAddresses(space, expectation.pattern, result) == expectation.expected);
        CHECK(result.matched == expectation.expected.size());
        CHECK_FALSE(result.error.has_value());
    }
}

TEST_CASE("visit reports a malformed pattern and visits nothing")
{
    AddressSpace<int> space;
    populate(space, { "/synth/1/freq" });
    const AddressSpace<int>& constSpace = space;
    const struct
    {
        const char* pattern;
        PatternError kind;
    } cases[] = {
        { "synth/1/freq", PatternError::MissingLeadingSlash },
        { "/synth/[1/freq", PatternError::UnterminatedClass },
        { "/synth/{1/freq", PatternError::UnterminatedBraces },
    };
    for (const auto& malformed : cases)
    {
        INFO("pattern " << malformed.pattern);
        std::size_t visits = 0;
        const oscpm::MatchResult mutableResult = space.visit(malformed.pattern, [&](std::string_view, int&)
            { ++visits; });
        const oscpm::MatchResult constResult = constSpace.visit(malformed.pattern, [&](std::string_view, const int&)
            { ++visits; });
        CHECK(visits == 0);
        for (const oscpm::MatchResult& result : { mutableResult, constResult })
        {
            CHECK(result.matched == 0);
            REQUIRE(result.error.has_value());
            CHECK(faults(result.error, malformed.kind));
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
    CHECK(matchingAddresses(space, repeated("/*", 64)) == Addresses { repeated("/a", 64) });
    CHECK(matchingAddresses(space, repeated("/*", 65)) == Addresses { repeated("/a", 64) + "/b" });
    CHECK(matchingAddresses(space, repeated("/*", 70)) == Addresses { deep });
    CHECK(matchingAddresses(space, repeated("/*", 70) + "/b") == Addresses { deep + "/b" });
    CHECK(matchingAddresses(space, repeated("/a", 69) + "//b") == Addresses { deep + "/b" });
    CHECK(matchingAddresses(space, "/a" + repeated("//a", 69)) == Addresses { deep });
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
    REQUIRE(ints.remove(parsedAddress("/synth/1/freq")));
    REQUIRE(mayThrow.remove(parsedAddress("/synth/1/freq")));
    for (const std::string& pattern : { std::string("/synth/*/freq"), std::string("//gain"), std::string("/*/2/{amp,freq}"), std::string("/a//b"), deep + "/*" })
    {
        INFO("pattern " << pattern);
        CHECK(matchingAddresses(mayThrow, pattern) == matchingAddresses(ints, pattern));
    }
}

TEST_CASE("visit reports a pattern longer than the supported length and visits nothing")
{
    AddressSpace<int> space;
    populate(space, { "/a" });
    std::size_t visits = 0;
    const oscpm::MatchResult result = space.visit("/*" + std::string(oscpm::kMaxPatternLength, 'a'), [&](std::string_view, int&)
        { ++visits; });
    CHECK(visits == 0);
    CHECK(result.matched == 0);
    REQUIRE(result.error.has_value());
    CHECK(std::string(oscpm::toString(*result.error)) == "PatternTooLong");
}

TEST_CASE("visit through a const space passes a const value")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2" });
    const AddressSpace<int>& constSpace = space;
    int sum = 0;
    const oscpm::MatchResult result = constSpace.visit("/a/*", [&](std::string_view, const int& value)
        { sum += value + 1; });
    CHECK(result.matched == 2);
    CHECK_FALSE(result.error.has_value());
    CHECK(sum == 2);
}

TEST_CASE("a repeated visit gives the same result and a change to the space is seen at once")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2" });
    const Addresses first = matchingAddresses(space, "/a/*");
    CHECK(matchingAddresses(space, "/a/*") == first);
    CHECK(first == Addresses { "/a/1", "/a/2" });

    REQUIRE(space.add(parsedAddress("/a/0"), 0));
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/0", "/a/1", "/a/2" });
    REQUIRE(space.remove(parsedAddress("/a/1")));
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/0", "/a/2" });
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/0", "/a/2" });
}

TEST_CASE("a single-bucket memo serves alternating patterns correctly")
{
    AddressSpace<int, true, 0, 4> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    for (int round = 0; round < 3; ++round)
    {
        CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
        CHECK(matchingAddresses(space, "/b/*") == Addresses { "/b/1" });
        CHECK(matchingAddresses(space, "/*/1") == Addresses { "/a/1", "/b/1" });
    }
}

TEST_CASE("a visitor may visit the same space while a memoised result is delivered")
{
    AddressSpace<int, true, 0, 4> space;
    populate(space, { "/a/1", "/a/2", "/a/3", "/b/1", "/b/2" });
    const auto nested = [&](std::string_view pattern)
    {
        Addresses outer;
        Addresses inner;
        oscpm::MatchResult innerResult { };
        const oscpm::MatchResult outerResult = space.visit("/a/*", [&](std::string_view address, int&)
            {
                if (outer.empty())
                {
                    inner = visitAddresses(space, pattern, innerResult);
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
    CHECK(matchingAddresses(space, "/b/*") == Addresses { "/b/1", "/b/2" });
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2", "/a/3" });
}

TEST_CASE("a result larger than the inline limit and a pattern longer than the memo limit are still delivered in full")
{
    AddressSpace<int, true, 2, 2> space;
    populate(space, { "/a/1", "/a/2", "/a/3", "/a/4" });
    for (int round = 0; round < 2; ++round)
    {
        CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2", "/a/3", "/a/4" });
        CHECK(matchingAddresses(space, "/a/[12]") == Addresses { "/a/1", "/a/2" });
    }
    const std::string longPattern = "/a/*" + std::string(oscpm::kMaxMemoPatternLength, '*');
    for (int round = 0; round < 2; ++round)
    {
        CHECK(matchingAddresses(space, longPattern) == Addresses { "/a/1", "/a/2", "/a/3", "/a/4" });
    }
}

TEST_CASE("a default space serves a result of 1024 matches from its memo unchanged")
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
        CHECK(matchingAddresses(space, "/voice/*") == voices);
        CHECK(matchingAddresses(space, "/master/*") == Addresses { "/master/gain", "/master/pan" });
    }
}

TEST_CASE("an address space without a memo behaves the same")
{
    AddressSpace<int, false> space;
    populate(space, { "/a/1", "/a/2", "/b/1" });
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(matchingAddresses(space, "/a/1") == Addresses { "/a/1" });
    REQUIRE(space.add(parsedAddress("/a/0"), 0));
    CHECK(matchingAddresses(space, "/a/*") == Addresses { "/a/0", "/a/1", "/a/2" });
}

TEST_CASE("visit sees every add and remove after a pattern has been visited")
{
    AddressSpace<int> space;
    populate(space, { "/a" });
    oscpm::MatchResult result { 0, std::nullopt };
    for (int round = 0; round < 2; ++round)
    {
        CHECK(visitAddresses(space, "/b", result) == Addresses { });
        CHECK(visitAddresses(space, "/*", result) == Addresses { "/a" });
    }
    REQUIRE(space.add(parsedAddress("/b"), 0));
    CHECK(visitAddresses(space, "/b", result) == Addresses { "/b" });
    CHECK(visitAddresses(space, "/*", result) == Addresses { "/a", "/b" });
    REQUIRE(space.remove(parsedAddress("/a")));
    CHECK(visitAddresses(space, "/a", result) == Addresses { });
    CHECK(visitAddresses(space, "/*", result) == Addresses { "/b" });
}

TEST_CASE("every registered address matches itself alone through interleaved adds and removes")
{
    AddressSpace<int> space;
    std::set<std::string> registered;
    std::mt19937 generator(3);
    for (int step = 0; step < 4000; ++step)
    {
        const std::string address = "/m/" + std::to_string(generator() % 600);
        if (generator() % 3 != 0)
        {
            space.add(parsedAddress(address), 0);
            registered.insert(address);
        }
        else
        {
            space.remove(parsedAddress(address));
            registered.erase(address);
        }
        if (step % 50 != 0)
        {
            continue;
        }
        for (const std::string& expected : registered)
        {
            oscpm::MatchResult result { };
            REQUIRE(visitAddresses(space, expected, result) == Addresses { expected });
            REQUIRE(space.find(expected) != nullptr);
        }
        oscpm::MatchResult absent { };
        CHECK(visitAddresses(space, "/m/600", absent).empty());
        CHECK(space.find("/m/600") == nullptr);
    }
}

TEST_CASE("the address index finds every address it holds and none it has erased through interleaved inserts and erases")
{
    oscpm::detail::AddressIndex index;
    Addresses sorted;
    std::mt19937 generator(11);
    for (int step = 0; step < 3000; ++step)
    {
        const std::string address = "/m/" + std::to_string(generator() % 400);
        const auto place = std::lower_bound(sorted.begin(), sorted.end(), address);
        const auto offset = static_cast<std::size_t>(place - sorted.begin());
        if (place != sorted.end() && *place == address)
        {
            sorted.erase(place);
            index.erase(offset);
        }
        else if (generator() % 3 != 0)
        {
            index.reserveForInsert();
            sorted.insert(place, address);
            index.insert(offset, oscpm::detail::hashOf(address));
        }
        if (step % 25 != 0)
        {
            continue;
        }
        for (std::size_t expected = 0; expected < sorted.size(); ++expected)
        {
            REQUIRE(indexedPosition(index, sorted, sorted[expected]) == expected);
        }
        REQUIRE_FALSE(indexedPosition(index, sorted, "/m/400").has_value());
    }
    CHECK_FALSE(sorted.empty());
}

TEST_CASE("a value type whose move may throw is visited exactly as an int is")
{
    AddressSpace<int> ints;
    AddressSpace<MayThrowOnMove> mayThrow;
    const Addresses addresses { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp", "/mixer/gain" };
    populate(ints, addresses);
    populate(mayThrow, addresses);
    REQUIRE(ints.remove(parsedAddress("/synth/1/freq")));
    REQUIRE(mayThrow.remove(parsedAddress("/synth/1/freq")));
    oscpm::MatchResult intsResult { 0, std::nullopt };
    oscpm::MatchResult mayThrowResult { 0, std::nullopt };
    for (int round = 0; round < 2; ++round)
    {
        for (const char* pattern : { "/synth/2/amp", "/synth/1/freq", "/synth/*/freq", "//gain", "/synth/[" })
        {
            INFO("pattern " << pattern);
            CHECK(visitAddresses(mayThrow, pattern, mayThrowResult) == visitAddresses(ints, pattern, intsResult));
            CHECK(mayThrowResult.matched == intsResult.matched);
            CHECK(mayThrowResult.error.has_value() == intsResult.error.has_value());
        }
    }
}

TEST_CASE("a copied address space and a moved-from one visit correctly")
{
    AddressSpace<int> source;
    populate(source, { "/a/1", "/a/2" });
    oscpm::MatchResult result { 0, std::nullopt };
    CHECK(visitAddresses(source, "/a/1", result) == Addresses { "/a/1" });
    CHECK(visitAddresses(source, "/a/*", result) == Addresses { "/a/1", "/a/2" });

    const AddressSpace<int> copy(source);
    REQUIRE(source.remove(parsedAddress("/a/1")));
    CHECK(visitAddresses(copy, "/a/1", result) == Addresses { "/a/1" });
    CHECK(visitAddresses(copy, "/a/*", result) == Addresses { "/a/1", "/a/2" });
    CHECK(visitAddresses(source, "/a/1", result) == Addresses { });
    CHECK(visitAddresses(source, "/a/*", result) == Addresses { "/a/2" });

    const AddressSpace<int> moved(std::move(source));
    CHECK(visitAddresses(moved, "/a/2", result) == Addresses { "/a/2" });
    CHECK(visitAddresses(source, "/a/2", result) == Addresses { });
    CHECK(visitAddresses(source, "/a/*", result) == Addresses { });
}

TEST_CASE("a space copied or moved during a visit starts outside any visit")
{
    AddressSpace<int> space;
    populate(space, { "/a/1", "/a/2" });
    Addresses copied;
    Addresses moved;
    space.visit("/a/1", [&](std::string_view, int&)
        {
            AddressSpace<int> copy(space);
            REQUIRE(copy.add(parsedAddress("/b"), 0));
            REQUIRE(copy.remove(parsedAddress("/a/2")));
            copied = allAddresses(copy);
            AddressSpace<int> taken(std::move(copy));
            REQUIRE(taken.add(parsedAddress("/c"), 0));
            moved = allAddresses(taken); });
    CHECK(copied == Addresses { "/a/1", "/b" });
    CHECK(moved == Addresses { "/a/1", "/b", "/c" });
    CHECK(allAddresses(space) == Addresses { "/a/1", "/a/2" });
}

TEST_CASE("a memo copied while it is being read accepts a new result afterwards")
{
    using Memo = oscpm::detail::DispatchMemo<0, 4, 64>;
    struct Method
    {
        std::string address;
        int value;
    };
    std::vector<Method> methods { { "/a", 1 }, { "/b", 2 } };
    Memo memo(true);
    const Memo::Results first { 0 };
    memo.store("/a", oscpm::detail::hashOf("/a"), first, 1);

    std::optional<Memo> copy;
    std::size_t numVisited = 0;
    const auto copyWhileReading = [&](std::string_view, int&)
    { copy.emplace(memo); };
    REQUIRE(memo.visit("/a", oscpm::detail::hashOf("/a"), methods, copyWhileReading, numVisited));
    REQUIRE(copy.has_value());

    const Memo::Results second { 1 };
    copy->store("/b", oscpm::detail::hashOf("/b"), second, 1);
    Addresses served;
    const auto record = [&](std::string_view address, int&)
    { served.emplace_back(address); };
    CHECK(copy->visit("/b", oscpm::detail::hashOf("/b"), methods, record, numVisited));
    CHECK(served == Addresses { "/b" });
}

TEST_CASE("a memo assigned to itself while it is being read is left as it was")
{
    using Memo = oscpm::detail::DispatchMemo<0, 4, 64>;
    struct Method
    {
        std::string address;
        int value;
    };
    std::vector<Method> methods { { "/a", 1 }, { "/b", 2 } };
    Memo memo(true);
    const Memo::Results first { 0 };
    memo.store("/a", oscpm::detail::hashOf("/a"), first, 1);

    std::size_t numVisited = 0;
    const auto assignToItself = [&](std::string_view, int&)
    {
        Memo& same = memo;
        memo = same;
    };
    REQUIRE(memo.visit("/a", oscpm::detail::hashOf("/a"), methods, assignToItself, numVisited));

    const Memo::Results second { 1 };
    memo.store("/b", oscpm::detail::hashOf("/b"), second, 1);
    Addresses served;
    const auto record = [&](std::string_view address, int&)
    { served.emplace_back(address); };
    CHECK(memo.visit("/b", oscpm::detail::hashOf("/b"), methods, record, numVisited));
    CHECK(served == Addresses { "/b" });
}

TEST_CASE("a moved address space keeps its values and the moved-from one stays usable")
{
    AddressSpace<int> source;
    populate(source, { "/a/1", "/a/2" });
    CHECK(matchingAddresses(source, "/a/*") == Addresses { "/a/1", "/a/2" });

    AddressSpace<int> constructed(std::move(source));
    CHECK(matchingAddresses(constructed, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(matchingAddresses(constructed, "/a/*") == Addresses { "/a/1", "/a/2" });
    CHECK(matchingAddresses(source, "/a/*") == Addresses { });
    REQUIRE(source.add(parsedAddress("/b"), 0));
    CHECK(matchingAddresses(source, "/*") == Addresses { "/b" });
    CHECK(matchingAddresses(source, "/*") == Addresses { "/b" });

    AddressSpace<int> assigned;
    assigned = std::move(constructed);
    CHECK(matchingAddresses(assigned, "/a/*") == Addresses { "/a/1", "/a/2" });
    REQUIRE(constructed.add(parsedAddress("/c"), 0));
    CHECK(matchingAddresses(constructed, "//") == Addresses { "/c" });
    CHECK(matchingAddresses(constructed, "//") == Addresses { "/c" });
}

TEST_CASE("a move-only value type is stored and reached through visit")
{
    AddressSpace<std::unique_ptr<int>> space;
    REQUIRE(space.add(parsedAddress("/a"), std::make_unique<int>(1)));
    REQUIRE(space.add(parsedAddress("/b"), std::make_unique<int>(2)));
    space.visit("/*", [](std::string_view, std::unique_ptr<int>& value)
        { *value *= 10; });
    int sum = 0;
    space.visit([&](std::string_view, const std::unique_ptr<int>& value)
        { sum += *value; });
    CHECK(sum == 30);
}

TEST_CASE("a visitor reaches every matching handler without allocating")
{
    using Handler = std::function<void(int)>;
    static_assert(std::is_nothrow_move_constructible_v<Handler> && std::is_nothrow_move_assignable_v<Handler>);
    AddressSpace<Handler> handlers;
    std::vector<std::pair<std::string_view, int>> calls;
    for (const char* address : { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp" })
    {
        REQUIRE(handlers.add(parsedAddress(address), [&calls, address](int argument)
            { calls.emplace_back(address, argument); }));
    }
    calls.reserve(8);
    const auto pass = [](std::string_view, Handler& handler)
    { handler(440); };
    const auto passConst = [](std::string_view, const Handler& handler)
    { handler(220); };

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::MatchResult wildcard = handlers.visit("/synth/*/freq", pass);
    const oscpm::MatchResult literal = handlers.visit("/synth/2/amp", pass);
    const oscpm::MatchResult absent = handlers.visit("/synth/3/freq", pass);
    const oscpm::MatchResult viaConst = std::as_const(handlers).visit("/synth/2/*", passConst);
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

TEST_CASE("dispatch calls every matching handler with the same arguments without allocating")
{
    using Handler = std::function<void(int, const std::string&)>;
    struct Call
    {
        std::string_view address;
        int argument;
        const std::string* name;
        bool nameIntact;
        bool operator==(const Call& other) const
        {
            return address == other.address && argument == other.argument && name == other.name && nameIntact == other.nameIntact;
        }
    };
    AddressSpace<Handler> handlers;
    std::vector<Call> calls;
    for (const char* address : { "/synth/1/freq", "/synth/2/freq", "/synth/2/amp" })
    {
        REQUIRE(handlers.add(parsedAddress(address), [&calls, address](int argument, const std::string& name)
            { calls.push_back(Call { address, argument, &name, name == "note" }); }));
    }
    calls.reserve(8);
    const std::string name = "note";

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::MatchResult wildcard = handlers.dispatch("/synth/*/freq", 440, name);
    const oscpm::MatchResult literal = handlers.dispatch("/synth/2/amp", 1, name);
    const oscpm::MatchResult absent = handlers.dispatch("/synth/3/freq", 0, name);
    const oscpm::MatchResult malformed = handlers.dispatch("/synth/[1/freq", 0, name);
    const oscpm::MatchResult viaConst = std::as_const(handlers).dispatch("/synth/2/*", 220, name);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(wildcard.matched == 2);
    CHECK(literal.matched == 1);
    CHECK(absent.matched == 0);
    CHECK(malformed.matched == 0);
    CHECK(faults(malformed.error, PatternError::UnterminatedClass));
    CHECK(viaConst.matched == 2);
    const std::vector<Call> expected {
        { "/synth/1/freq", 440, &name, true }, { "/synth/2/freq", 440, &name, true }, { "/synth/2/amp", 1, &name, true },
        { "/synth/2/amp", 220, &name, true }, { "/synth/2/freq", 220, &name, true }
    };
    CHECK(calls == expected);

    calls.clear();
    std::string movable = "note";
    handlers.dispatch("/synth/*/freq", 880, std::move(movable));
    REQUIRE(calls.size() == 2);
    CHECK(calls[0].name == &movable);
    CHECK(calls[1].name == &movable);
    CHECK(calls[0].nameIntact);
    CHECK(calls[1].nameIntact);
}

TEST_CASE("dispatch hands every handler that takes an argument by value its own copy, even of an rvalue")
{
    using Handler = std::function<void(std::string)>;
    AddressSpace<Handler> handlers;
    std::vector<std::string> received;
    for (const char* address : { "/synth/1/freq", "/synth/2/freq" })
    {
        REQUIRE(handlers.add(parsedAddress(address), [&received](std::string name)
            { received.push_back(std::move(name)); }));
    }

    std::string name = "a name too long for the small string buffer";
    const std::string expected = name;
    const oscpm::MatchResult result = handlers.dispatch("/synth/*/freq", std::move(name));

    CHECK(result.matched == 2);
    CHECK(received == std::vector<std::string> { expected, expected });
}

TEST_CASE("dispatch calls a member function value on the object passed first, without allocating")
{
    struct Synth
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
        void setFrequency(float value)
        {
            frequency = value;
        }
        void setAmplitude(float value)
        {
            amplitude = value;
        }
        void report(std::vector<float>& values) const
        {
            values.push_back(frequency);
        }
    };
    AddressSpace<void (Synth::*)(float)> setters;
    REQUIRE(setters.add(parsedAddress("/synth/freq"), &Synth::setFrequency));
    REQUIRE(setters.add(parsedAddress("/synth/amp"), &Synth::setAmplitude));
    AddressSpace<void (Synth::*)(std::vector<float>&) const> reporters;
    REQUIRE(reporters.add(parsedAddress("/synth/report"), &Synth::report));
    Synth synth;
    std::vector<float> reported;
    reported.reserve(1);

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::MatchResult byReference = setters.dispatch("/synth/freq", synth, 440.0f);
    const oscpm::MatchResult byPointer = setters.dispatch("/synth/amp", &synth, 0.5f);
    const oscpm::MatchResult viaConst = std::as_const(reporters).dispatch("/synth/*", std::as_const(synth), reported);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(byReference.matched == 1);
    CHECK(byPointer.matched == 1);
    CHECK(viaConst.matched == 1);
    CHECK(synth.frequency == 440.0f);
    CHECK(synth.amplitude == 0.5f);
    CHECK(reported == std::vector<float> { 440.0f });
}

TEST_CASE("visit and find allocate nothing")
{
    AddressSpace<int> space;
    Addresses addresses;
    for (int i = 0; i < 100; ++i)
    {
        addresses.push_back("/synth/" + std::to_string(i) + "/freq");
    }
    populate(space, addresses);
    const std::string unmemoised = "/synth/*/freq" + std::string(oscpm::kMaxMemoPatternLength, '*');
    std::size_t visited = 0;
    const auto count = [&](std::string_view, const int&)
    { ++visited; };

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::MatchResult literalResult = space.visit("/synth/42/freq", count);
    const oscpm::MatchResult wildcardResult = space.visit("/synth/?/freq", count);
    const oscpm::MatchResult wildcardAgainResult = space.visit("/synth/?/freq", count);
    const oscpm::MatchResult unmemoisedResult = space.visit(unmemoised, count);
    space.visit(count);
    const oscpm::MatchResult malformedResult = space.visit("/synth/[4/freq", count);
    const oscpm::MatchResult absentResult = space.visit("/synth/42/gain", count);
    const oscpm::MatchResult absentAgainResult = space.visit("/synth/42/gain", count);
    const int* const found = space.find("/synth/42/freq");
    const int* const notFound = space.find("/synth/42/gain");
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(visited == 1 + 10 + 10 + 100 + 100);
    CHECK(literalResult.matched == 1);
    CHECK(wildcardResult.matched == 10);
    CHECK(wildcardAgainResult.matched == 10);
    CHECK(unmemoisedResult.matched == 100);
    CHECK(malformedResult.matched == 0);
    CHECK(malformedResult.error.has_value());
    CHECK(absentResult.matched == 0);
    CHECK(absentAgainResult.matched == 0);
    CHECK(found != nullptr);
    CHECK(notFound == nullptr);
}

TEST_CASE("every well-formed corpus pattern is delivered exactly as matches says")
{
    AddressSpace<int> space;
    std::set<std::string> registered;
    std::vector<std::string> patterns;
    std::vector<oscpm_test::CorpusCase> malformed;
    for (const oscpm_test::CorpusCase& corpusCase : oscpm_test::loadCorpus(OSCPM_CORPUS_PATH))
    {
        const oscpm::Expected<Address, oscpm::AddressError> candidate = Address::parse(corpusCase.address);
        if (candidate && registered.count(corpusCase.address) == 0 && space.add(*candidate, 0))
        {
            registered.insert(corpusCase.address);
        }
        if (Pattern::parse(corpusCase.pattern))
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
        oscpm::MatchResult result { 0, std::nullopt };
        CHECK(visitAddresses(space, pattern, result) == expected);
        CHECK(result.matched == expected.size());
        CHECK_FALSE(result.error.has_value());
    }
    for (const oscpm_test::CorpusCase& corpusCase : malformed)
    {
        INFO("corpus line " << corpusCase.line << ": " << corpusCase.text);
        oscpm::MatchResult result { 0, std::nullopt };
        CHECK(visitAddresses(space, corpusCase.pattern, result).empty());
        CHECK(result.matched == 0);
        REQUIRE(result.error.has_value());
        CHECK(std::string(oscpm::toString(*result.error)) == corpusCase.errorName);
    }
}

TEST_CASE("random adds, removes and visits agree with the standalone matcher")
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
            CHECK(space.add(parsedAddress(address), step) == fresh);
        }
        else if (operation == 1)
        {
            const std::string address = randomAddress();
            const bool present = model.erase(address) > 0;
            CHECK(space.remove(parsedAddress(address)) == present);
        }
        else
        {
            const std::string pattern = randomPattern();
            INFO("step " << step << " pattern " << pattern);
            CHECK(matchingAddresses(space, pattern) == expectedMatches(model, pattern));
        }
        CHECK(allAddresses(space) == Addresses(model.begin(), model.end()));
    }
}
