#include <oscpm/address_space.hpp>

#include "allocation_counter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// A literal Pattern for exact Lookup; every text here is well-formed.
oscpm::Pattern pattern(std::string_view text)
{
    const oscpm::ParseResult parsed = oscpm::Pattern::parse(text);
    REQUIRE(parsed.ok());
    return parsed.pattern();
}

} // namespace

TEST_CASE("AddressSpace stores a move-only value type", "[address_space]")
{
    oscpm::AddressSpace<std::unique_ptr<int>> space;
    CHECK_FALSE(space.add("/synth/freq", std::make_unique<int>(440)).has_value());

    int seen = 0;
    const std::size_t count = space.lookup(pattern("/synth/freq"),
                                           [&](std::string_view address, std::unique_ptr<int>& value) {
                                               CHECK(address == "/synth/freq");
                                               seen = *value;
                                           });
    CHECK(count == 1);
    CHECK(seen == 440);
}

TEST_CASE("AddressSpace stores a capturing lambda type", "[address_space]")
{
    int calls = 0;
    auto count = [&calls] { ++calls; };
    oscpm::AddressSpace<decltype(count)> space;
    CHECK_FALSE(space.add("/synth/freq", std::move(count)).has_value());

    space.lookup(pattern("/synth/freq"), [](std::string_view, decltype(count)& value) { value(); });
    CHECK(calls == 1);
}

TEST_CASE("add rejects a Malformed Address with its kind and offset", "[address_space]")
{
    oscpm::AddressSpace<int> space;

    const std::optional<oscpm::Error> wildcard = space.add("/synth/*", 1);
    REQUIRE(wildcard.has_value());
    CHECK(wildcard->kind == oscpm::ErrorKind::IllegalCharacter);
    CHECK(wildcard->offset == 7);

    const std::optional<oscpm::Error> braces = space.add("/{a,b}", 1);
    REQUIRE(braces.has_value());
    CHECK(braces->kind == oscpm::ErrorKind::IllegalCharacter);
    CHECK(braces->offset == 1);

    const std::optional<oscpm::Error> descendant = space.add("/a//b", 1);
    REQUIRE(descendant.has_value());
    CHECK(descendant->kind == oscpm::ErrorKind::EmptyPart);
    CHECK(descendant->offset == 3);

    const std::optional<oscpm::Error> trailing = space.add("/synth/", 1);
    REQUIRE(trailing.has_value());
    CHECK(trailing->kind == oscpm::ErrorKind::TrailingSlash);
    CHECK(trailing->offset == 6);

    const std::optional<oscpm::Error> noSlash = space.add("synth", 1);
    REQUIRE(noSlash.has_value());
    CHECK(noSlash->kind == oscpm::ErrorKind::MissingLeadingSlash);
    CHECK(noSlash->offset == 0);

    const std::optional<oscpm::Error> root = space.add("/", 1);
    REQUIRE(root.has_value());
    CHECK(root->kind == oscpm::ErrorKind::BareRoot);

    // Nothing was registered by the rejected calls.
    int visits = 0;
    space.forEach([&](std::string_view, int&) { ++visits; });
    CHECK(visits == 0);
}

TEST_CASE("add rejects a second registration at the same Address", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());

    const std::optional<oscpm::Error> duplicate = space.add("/synth/freq", 2);
    REQUIRE(duplicate.has_value());
    CHECK(duplicate->kind == oscpm::ErrorKind::Duplicate);
    CHECK(std::string_view(oscpm::toString(duplicate->kind)) == "Duplicate");

    // The first value stays; a typo never silently replaces a Method.
    int seen = 0;
    space.lookup(pattern("/synth/freq"),
                 [&](std::string_view, int& value) { seen = value; });
    CHECK(seen == 1);
}

TEST_CASE("remove reports NotFound for an unknown Address", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());

    const std::optional<oscpm::Error> unknown = space.remove("/synth/amp");
    REQUIRE(unknown.has_value());
    CHECK(unknown->kind == oscpm::ErrorKind::NotFound);
    CHECK(std::string_view(oscpm::toString(unknown->kind)) == "NotFound");

    // A Container that is not also a Method is not a registered Address.
    const std::optional<oscpm::Error> container = space.remove("/synth");
    REQUIRE(container.has_value());
    CHECK(container->kind == oscpm::ErrorKind::NotFound);

    // A deeper Address under a Method is unknown too.
    const std::optional<oscpm::Error> deeper = space.remove("/synth/freq/x");
    REQUIRE(deeper.has_value());
    CHECK(deeper->kind == oscpm::ErrorKind::NotFound);

    // A Malformed Address is reported as such rather than as NotFound.
    const std::optional<oscpm::Error> malformed = space.remove("/synth/?");
    REQUIRE(malformed.has_value());
    CHECK(malformed->kind == oscpm::ErrorKind::IllegalCharacter);
    CHECK(malformed->offset == 7);
}

TEST_CASE("remove unregisters a Method and a second remove reports NotFound", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());

    CHECK_FALSE(space.remove("/synth/freq").has_value());
    CHECK(space.lookup(pattern("/synth/freq"),
                       [](std::string_view, int&) {}) == 0);

    const std::optional<oscpm::Error> again = space.remove("/synth/freq");
    REQUIRE(again.has_value());
    CHECK(again->kind == oscpm::ErrorKind::NotFound);

    // The Address can be registered afresh.
    CHECK_FALSE(space.add("/synth/freq", 2).has_value());
    int seen = 0;
    space.lookup(pattern("/synth/freq"),
                 [&](std::string_view, int& value) { seen = value; });
    CHECK(seen == 2);
}

TEST_CASE("an Address may be both a Container and a Method", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 2).has_value());
    REQUIRE_FALSE(space.add("/synth", 1).has_value());

    int seen = 0;
    CHECK(space.lookup(pattern("/synth"),
                       [&](std::string_view address, int& value) {
                           CHECK(address == "/synth");
                           seen = value;
                       })
          == 1);
    CHECK(seen == 1);
    CHECK(space.lookup(pattern("/synth/freq"),
                       [&](std::string_view address, int& value) {
                           CHECK(address == "/synth/freq");
                           seen = value;
                       })
          == 1);
    CHECK(seen == 2);

    // Removing the Method at the Container leaves its children intact.
    REQUIRE_FALSE(space.remove("/synth").has_value());
    CHECK(space.lookup(pattern("/synth"), [](std::string_view, int&) {})
          == 0);
    CHECK(space.lookup(pattern("/synth/freq"),
                       [&](std::string_view, int& value) { seen = value; })
          == 1);
    CHECK(seen == 2);

    // And the Container can become a Method again.
    CHECK_FALSE(space.add("/synth", 3).has_value());
    CHECK(space.lookup(pattern("/synth"),
                       [&](std::string_view, int& value) { seen = value; })
          == 1);
    CHECK(seen == 3);
}

TEST_CASE("lookup of an unregistered exact Address returns 0 without calling the visitor",
          "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());
    REQUIRE_FALSE(space.add("/synth/amp/gain", 2).has_value());

    int calls = 0;
    const auto count = [&](std::string_view text) {
        return space.lookup(pattern(text), [&](std::string_view, int&) { ++calls; });
    };
    CHECK(count("/synth/amp") == 0);       // A Container only.
    CHECK(count("/synth/freq/x") == 0);    // Below a Method.
    CHECK(count("/synth/fre") == 0);       // A prefix of a Part.
    CHECK(count("/synth/freqs") == 0);     // A Part as a prefix.
    CHECK(count("/synt/freq") == 0);
    CHECK(count("/other") == 0);
    CHECK(count("/synth/FREQ") == 0);      // Case-sensitive.
    CHECK(calls == 0);

    const oscpm::AddressSpace<int> empty;
    CHECK(empty.lookup(pattern("/a"),
                       [&](std::string_view, const int&) { ++calls; })
          == 0);
    CHECK(calls == 0);
}

TEST_CASE("forEach visits Methods in address order whatever the registration order",
          "[address_space]")
{
    oscpm::AddressSpace<int> space;
    int next = 0;
    for (const char* address : {"/synth/2/freq", "/mixer", "/synth/1/amp", "/synth",
                                "/synth/10/freq", "/a", "/synth/1/freq", "/z/b", "/z/a"}) {
        REQUIRE_FALSE(space.add(address, next++).has_value());
    }

    std::vector<std::string> visited;
    space.forEach([&](std::string_view address, int&) { visited.emplace_back(address); });

    // Parts compare bytewise, so "1" < "10" < "2", and a Method precedes
    // the Methods beneath it.
    const std::vector<std::string> expected = {
        "/a",           "/mixer",         "/synth",        "/synth/1/amp", "/synth/1/freq",
        "/synth/10/freq", "/synth/2/freq", "/z/a",          "/z/b",
    };
    CHECK(visited == expected);
}

TEST_CASE("address order compares Part by Part rather than whole strings", "[address_space]")
{
    // '-' (0x2D) and '.' (0x2E) sort before '/' (0x2F) as bytes, so a whole-
    // string comparison would put "/a-b" and "/a.b" before "/a/b". Address
    // order is the tree's order: the Part "a" precedes the Part "a-b".
    oscpm::AddressSpace<int> space;
    for (const char* address : {"/a-b", "/a/b", "/a.b"}) {
        REQUIRE_FALSE(space.add(address, 0).has_value());
    }
    std::vector<std::string> visited;
    space.forEach([&](std::string_view address, int&) { visited.emplace_back(address); });
    CHECK(visited == std::vector<std::string>{"/a/b", "/a-b", "/a.b"});
}

TEST_CASE("forEach and lookup pass a mutable reference to the stored value", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());
    REQUIRE_FALSE(space.add("/synth/amp", 1).has_value());

    space.forEach([](std::string_view, int& value) { value *= 10; });
    space.lookup(pattern("/synth/amp"),
                 [](std::string_view, int& value) { value += 1; });

    std::vector<int> values;
    space.forEach([&](std::string_view, int& value) { values.push_back(value); });
    CHECK(values == std::vector<int>{11, 10});
}

TEST_CASE("const lookup and forEach pass a const reference", "[address_space]")
{
    oscpm::AddressSpace<int> mutableSpace;
    REQUIRE_FALSE(mutableSpace.add("/synth/freq", 7).has_value());
    const oscpm::AddressSpace<int>& space = mutableSpace;

    int seen = 0;
    CHECK(space.lookup(pattern("/synth/freq"),
                       [&](std::string_view address, const int& value) {
                           CHECK(address == "/synth/freq");
                           seen = value;
                       })
          == 1);
    CHECK(seen == 7);

    seen = 0;
    space.forEach([&](std::string_view, const int& value) { seen = value; });
    CHECK(seen == 7);

    // The visitor's parameter must accept a const value.
    const auto constVisitor = [](std::string_view, const int&) {};
    STATIC_REQUIRE(std::is_invocable_v<decltype(constVisitor), std::string_view, const int&>);
}

TEST_CASE("lookup and forEach do not allocate", "[address_space][realtime]")
{
    oscpm::AddressSpace<std::unique_ptr<int>> space;
    for (const char* address : {"/synth/1/freq", "/synth/1/amp", "/synth/2/freq", "/synth",
                                "/mixer/master/gain", "/a/very/deeply/nested/method"}) {
        REQUIRE_FALSE(space.add(address, std::make_unique<int>(1)).has_value());
    }
    const oscpm::Pattern hit = pattern("/synth/2/freq");
    const oscpm::Pattern miss = pattern("/synth/3/freq");
    const oscpm::Pattern wild = pattern("/synth/*/freq");
    int visits = 0;
    const auto visit = [&](std::string_view, std::unique_ptr<int>&) { ++visits; };

    const std::size_t before = oscpm_test::allocationCount();

    const std::size_t found = space.lookup(hit, visit);
    const std::size_t notFound = space.lookup(miss, visit);
    space.lookup(wild, visit); // Pending ticket 05; only its allocation is asserted.
    space.forEach(visit);
    const std::size_t constFound =
        std::as_const(space).lookup(hit, [&](std::string_view, const std::unique_ptr<int>&) {
            ++visits;
        });
    std::as_const(space).forEach([&](std::string_view, const std::unique_ptr<int>&) { ++visits; });

    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(found == 1);
    CHECK(notFound == 0);
    CHECK(constFound == 1);
    CHECK(visits == 1 + 6 + 1 + 6); // The wildcard Lookup finds nothing yet.
}

// Wildcard Lookup is ticket 05. Until then a Pattern with Wildcards or the
// Descendant Operator is accepted and finds nothing, even at an Address
// the Pattern would Match.
TEST_CASE("lookup with a Wildcard Pattern is accepted and pending ticket 05", "[address_space]")
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/freq", 1).has_value());
    REQUIRE_FALSE(space.add("/synth/amp", 2).has_value());

    int calls = 0;
    for (const char* pattern : {"/synth/*", "/synth/?req", "/synth/[fa]*", "/synth/{freq,amp}",
                                "//freq", "/synth//amp"}) {
        INFO(pattern);
        const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
        REQUIRE(parsed.ok());
        CHECK(space.lookup(parsed.pattern(), [&](std::string_view, int&) { ++calls; }) == 0);
    }
    CHECK(calls == 0);
}
