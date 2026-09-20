/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"
#include "corpus.h"

#include <oscpm/address_space.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
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
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(visited == 1 + 10 + 10 + 100 + 100);
}

TEST_CASE("every well-formed corpus pattern is delivered exactly as matches says")
{
    AddressSpace<int> space;
    std::set<std::string> registered;
    std::vector<std::string> patterns;
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
    }
    REQUIRE(space.size() == registered.size());
    for (const std::string& pattern : patterns)
    {
        INFO("pattern " << pattern);
        CHECK(lookupAddresses(space, pattern) == expectedMatches(registered, pattern));
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
