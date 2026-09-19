// Lookup with Wildcards and the Descendant Operator: the tree walk must
// give the same answers as the standalone matcher, visit each Method once,
// in address order, and never allocate.

#include <oscpm/address_space.hpp>

#include "allocation_counter.hpp"
#include "corpus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

oscpm::Pattern pattern(std::string_view text)
{
    const oscpm::ParseResult parsed = oscpm::Pattern::parse(text);
    INFO(text);
    REQUIRE(parsed.ok());
    return parsed.pattern();
}

oscpm::AddressSpace<int> spaceOf(std::initializer_list<const char*> addresses)
{
    oscpm::AddressSpace<int> space;
    int next = 0;
    for (const char* address : addresses) {
        INFO(address);
        REQUIRE_FALSE(space.add(address, next++).has_value());
    }
    return space;
}

// Every Address `lookup` delivers for `text`, in the order delivered.
std::vector<std::string> found(oscpm::AddressSpace<int>& space, std::string_view text)
{
    std::vector<std::string> visited;
    const std::size_t count = space.lookup(
        pattern(text), [&](std::string_view address, int&) { visited.emplace_back(address); });
    CHECK(count == visited.size());
    return visited;
}

// Every Address in `space` in address order, as forEach gives them.
std::vector<std::string> all(oscpm::AddressSpace<int>& space)
{
    std::vector<std::string> visited;
    space.forEach([&](std::string_view address, int&) { visited.emplace_back(address); });
    return visited;
}

} // namespace

TEST_CASE("lookup replays the conformance corpus through an Address Space",
          "[address_space][corpus]")
{
    // Every well-formed corpus Address is registered, then every well-formed
    // corpus Pattern is looked up: the walk must deliver exactly the
    // registered Addresses the matcher says it Matches, in address order.
    const std::vector<oscpm_test::Case> cases = oscpm_test::loadCorpus(OSCPM_CORPUS_PATH);

    oscpm::AddressSpace<int> space;
    std::set<std::string> patterns;
    for (const oscpm_test::Case& c : cases) {
        if (!oscpm::validateAddress(c.address)) {
            const std::optional<oscpm::Error> error = space.add(c.address, c.line);
            CHECK((!error || error->kind == oscpm::ErrorKind::Duplicate));
        }
        if (oscpm::Pattern::parse(c.pattern).ok()) {
            patterns.insert(c.pattern);
        }
    }
    const std::vector<std::string> addresses = all(space);
    REQUIRE(addresses.size() > 100); // A thin corpus would prove little.

    for (const std::string& text : patterns) {
        INFO("pattern " << text);
        const oscpm::Pattern p = pattern(text);

        std::vector<std::string> expected;
        for (const std::string& address : addresses) {
            if (p.matches(address) == oscpm::MatchResult::Match) {
                expected.push_back(address);
            }
        }

        std::vector<std::string> visited;
        const std::size_t count = space.lookup(
            p, [&](std::string_view address, int&) { visited.emplace_back(address); });
        CHECK(count == expected.size());
        CHECK(visited == expected);
    }
}

TEST_CASE("each within-Part Wildcard selects the right Methods", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/1/freq", "/synth/12/freq",
                                              "/synth/2/amp", "/synth/freq", "/mixer/freq",
                                              "/synth/1"});

    CHECK(found(space, "/synth/?/freq") == std::vector<std::string>{"/synth/1/freq"});
    CHECK(found(space, "/synth/*/freq")
          == std::vector<std::string>{"/synth/1/freq", "/synth/12/freq"});
    CHECK(found(space, "/synth/*") == std::vector<std::string>{"/synth/1", "/synth/freq"});
    CHECK(found(space, "/synth/[0-9]/*")
          == std::vector<std::string>{"/synth/1/freq", "/synth/2/amp"});
    CHECK(found(space, "/synth/[!1]*/*") == std::vector<std::string>{"/synth/2/amp"});
    CHECK(found(space, "/{synth,mixer}/freq")
          == std::vector<std::string>{"/mixer/freq", "/synth/freq"});
    CHECK(found(space, "/synth/{1,12,3}/freq")
          == std::vector<std::string>{"/synth/1/freq", "/synth/12/freq"});
    CHECK(found(space, "/*/*/*")
          == std::vector<std::string>{"/synth/1/freq", "/synth/12/freq", "/synth/2/amp"});
    CHECK(found(space, "/nothing/*").empty());
    CHECK(found(space, "/synth/1/fre?") == std::vector<std::string>{"/synth/1/freq"});
    CHECK(found(space, "/synth/1/freq?").empty());
}

TEST_CASE("the Descendant Operator reaches Methods at any depth", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/gain", "/a/gain", "/a/b/gain", "/a/b/c/gain",
                                              "/a/b", "/a/gain/x", "/b/gain"});

    CHECK(found(space, "//gain")
          == std::vector<std::string>{"/a/b/c/gain", "/a/b/gain", "/a/gain", "/b/gain", "/gain"});
    CHECK(found(space, "/a//gain")
          == std::vector<std::string>{"/a/b/c/gain", "/a/b/gain", "/a/gain"});
    CHECK(found(space, "/a//b") == std::vector<std::string>{"/a/b"});
    CHECK(found(space, "//a//gain")
          == std::vector<std::string>{"/a/b/c/gain", "/a/b/gain", "/a/gain"});
    CHECK(found(space, "//gain/x") == std::vector<std::string>{"/a/gain/x"});
    CHECK(found(space, "//[bc]/gain")
          == std::vector<std::string>{"/a/b/c/gain", "/a/b/gain", "/b/gain"});
    CHECK(found(space, "//{a,b}/*")
          == std::vector<std::string>{"/a/b", "/a/b/gain", "/a/gain", "/b/gain"});
    CHECK(found(space, "//x") == std::vector<std::string>{"/a/gain/x"});
    CHECK(found(space, "//y").empty());
}

TEST_CASE("lookup backtracks past a false first match", "[address_space]")
{
    // The first "b" after "a" is not the one followed by "c"; the walk must
    // let "//" absorb it and try the later "b".
    oscpm::AddressSpace<int> space = spaceOf({"/a/b/x/b/c", "/a/b/c", "/a/b/x/c", "/a/b/x"});
    CHECK(found(space, "/a//b/c") == std::vector<std::string>{"/a/b/c", "/a/b/x/b/c"});
    CHECK(found(space, "//b//c")
          == std::vector<std::string>{"/a/b/c", "/a/b/x/b/c", "/a/b/x/c"});

    // The same for '*' inside a Part.
    oscpm::AddressSpace<int> stars = spaceOf({"/aXbYb", "/abb", "/ab", "/ba"});
    CHECK(found(stars, "/a*b") == std::vector<std::string>{"/aXbYb", "/ab", "/abb"});
    CHECK(found(stars, "/*b*b") == std::vector<std::string>{"/aXbYb", "/abb"});
    CHECK(found(stars, "/{a,ab}b") == std::vector<std::string>{"/ab", "/abb"});
}

TEST_CASE("each matching Method is visited exactly once", "[address_space]")
{
    // "//a//b" can reach "/a/a/b" with either "//" absorbing the first "a".
    oscpm::AddressSpace<int> space = spaceOf({"/a/a/b", "/a/b", "/a/a/a/b", "/a/a/b/b"});
    CHECK(found(space, "//a//b")
          == std::vector<std::string>{"/a/a/a/b", "/a/a/b", "/a/a/b/b", "/a/b"});
    CHECK(found(space, "//b") == std::vector<std::string>{"/a/a/a/b", "/a/a/b", "/a/a/b/b", "/a/b"});
    CHECK(found(space, "//a//a//b") == std::vector<std::string>{"/a/a/a/b", "/a/a/b", "/a/a/b/b"});
    CHECK(found(space, "//*//*") == std::vector<std::string>{"/a/a/a/b", "/a/a/b", "/a/a/b/b", "/a/b"});
}

TEST_CASE("wildcard lookup visits Methods in address order", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/2/freq", "/mixer/freq", "/synth/1/amp",
                                              "/synth", "/synth/10/freq", "/synth/1/freq",
                                              "/synth/1", "/a-b/freq", "/a/b/freq"});
    const std::vector<std::string> everything = all(space);
    CHECK(found(space, "//*") == everything);

    std::vector<std::string> expected;
    for (const std::string& address : everything) {
        if (address.size() >= 5 && address.compare(address.size() - 5, 5, "/freq") == 0) {
            expected.push_back(address);
        }
    }
    CHECK(found(space, "//freq") == expected);
    CHECK(found(space, "/synth/*/*")
          == std::vector<std::string>{"/synth/1/amp", "/synth/1/freq", "/synth/10/freq",
                                      "/synth/2/freq"});
}

TEST_CASE("a Method that is also a Container is visited before its children", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/1/freq", "/synth/1", "/synth"});
    CHECK(found(space, "//*") == std::vector<std::string>{"/synth", "/synth/1", "/synth/1/freq"});
    CHECK(found(space, "/synth//*") == std::vector<std::string>{"/synth/1", "/synth/1/freq"});
    CHECK(found(space, "/synth//1") == std::vector<std::string>{"/synth/1"});
}

TEST_CASE("wildcard lookup passes a mutable reference and a const overload", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/1/freq", "/synth/2/freq", "/synth/2/amp"});
    space.lookup(pattern("/synth/*/freq"), [](std::string_view, int& value) { value = 100; });

    const oscpm::AddressSpace<int>& constSpace = space;
    std::vector<int> values;
    const std::size_t count = constSpace.lookup(
        pattern("//*"), [&](std::string_view, const int& value) { values.push_back(value); });
    CHECK(count == 3);
    CHECK(values == std::vector<int>{100, 2, 100});
}

TEST_CASE("the span convenience collects matches into caller storage", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/1/freq", "/synth/2/freq", "/synth/2/amp"});

    oscpm::Method<int> out[4];
    const oscpm::LookupCount count = space.lookup(pattern("/synth/*/freq"), out, 4);
    CHECK(count.stored == 2);
    CHECK(count.matched == 2);
    CHECK(out[0].address == "/synth/1/freq");
    CHECK(*out[0].value == 0);
    CHECK(out[1].address == "/synth/2/freq");
    CHECK(*out[1].value == 1);

    // The pointer refers to the stored value, not a copy.
    *out[1].value = 42;
    int seen = 0;
    space.lookup(pattern("/synth/2/freq"), [&](std::string_view, int& value) { seen = value; });
    CHECK(seen == 42);

    // A Wildcard-free Pattern takes the same route.
    const oscpm::LookupCount exact = space.lookup(pattern("/synth/2/amp"), out, 4);
    CHECK(exact.stored == 1);
    CHECK(exact.matched == 1);
    CHECK(out[0].address == "/synth/2/amp");

    // The const overload hands out pointers to const.
    const oscpm::AddressSpace<int>& constSpace = space;
    oscpm::Method<const int> constOut[4];
    const oscpm::LookupCount constCount = constSpace.lookup(pattern("//amp"), constOut, 4);
    CHECK(constCount.stored == 1);
    CHECK(constOut[0].address == "/synth/2/amp");
    CHECK(*constOut[0].value == 2);
}

TEST_CASE("the span convenience truncates when storage is too small", "[address_space]")
{
    oscpm::AddressSpace<int> space = spaceOf({"/synth/1/freq", "/synth/2/freq", "/synth/3/freq",
                                              "/synth/4/freq"});

    oscpm::Method<int> out[2] = {};
    const oscpm::LookupCount count = space.lookup(pattern("/synth/*/freq"), out, 2);
    CHECK(count.stored == 2);
    CHECK(count.matched == 4);
    CHECK(out[0].address == "/synth/1/freq");
    CHECK(out[1].address == "/synth/2/freq");

    // Zero capacity still reports the true count and writes nothing.
    const oscpm::LookupCount none = space.lookup(pattern("/synth/*/freq"), nullptr, 0);
    CHECK(none.stored == 0);
    CHECK(none.matched == 4);

    // No Match writes nothing.
    const oscpm::LookupCount miss = space.lookup(pattern("/mixer/*"), out, 2);
    CHECK(miss.stored == 0);
    CHECK(miss.matched == 0);
    CHECK(out[0].address == "/synth/1/freq"); // Untouched.
}

TEST_CASE("wildcard lookup does not allocate, including deep Descendant walks",
          "[address_space][realtime]")
{
    oscpm::AddressSpace<std::unique_ptr<int>> space;
    std::string deep;
    for (int i = 0; i < 40; ++i) {
        deep += "/a";
        REQUIRE_FALSE(space.add(deep, std::make_unique<int>(i)).has_value());
        REQUIRE_FALSE(space.add(deep + "/b", std::make_unique<int>(i)).has_value());
        REQUIRE_FALSE(space.add(deep + "/gain", std::make_unique<int>(i)).has_value());
    }
    for (const char* address : {"/synth/1/freq", "/synth/2/freq", "/mixer/master/gain"}) {
        REQUIRE_FALSE(space.add(address, std::make_unique<int>(0)).has_value());
    }

    const oscpm::Pattern patterns[] = {
        pattern("//gain"),          pattern("//a//a//b"),      pattern("/a//b/gain"),
        pattern("/synth/*/freq"),   pattern("/{synth,mixer}/?/*"), pattern("//[ab]*"),
        pattern("/a/a/a//a/a/a/a/a/a/a/b"),
    };
    constexpr std::size_t patternCount = std::size(patterns);
    std::size_t visits = 0;
    const auto visit = [&](std::string_view, std::unique_ptr<int>&) { ++visits; };
    oscpm::Method<std::unique_ptr<int>> out[8];

    // Catch2 assertions allocate, so the counts are gathered first and
    // checked after the window closes.
    std::size_t visited[patternCount];
    std::size_t spanMatched[patternCount];
    std::size_t constVisited[patternCount];
    const std::size_t before = oscpm_test::allocationCount();
    for (std::size_t i = 0; i < patternCount; ++i) {
        visited[i] = space.lookup(patterns[i], visit);
        spanMatched[i] = space.lookup(patterns[i], out, 8).matched;
        constVisited[i] = std::as_const(space).lookup(
            patterns[i], [&](std::string_view, const std::unique_ptr<int>&) { ++visits; });
    }
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    std::size_t total = 0;
    for (std::size_t i = 0; i < patternCount; ++i) {
        INFO(patterns[i].text());
        CHECK(spanMatched[i] == visited[i]);
        CHECK(constVisited[i] == visited[i]);
        total += 2 * visited[i];
    }
    CHECK(visits == total);
    CHECK(space.lookup(pattern("//gain"), visit) == 41);
}
