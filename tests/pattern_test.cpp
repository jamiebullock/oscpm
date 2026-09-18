#include <oscpm/pattern.hpp>

#include "allocation_counter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>
#include <type_traits>

TEST_CASE("Pattern is a trivially copyable non-owning view", "[pattern]")
{
    STATIC_REQUIRE(std::is_trivially_copyable_v<oscpm::Pattern>);
}

TEST_CASE("parse and matches do not allocate", "[pattern][realtime]")
{
    const std::string_view pattern = "/synth/1/freq";
    const std::string_view address = "/synth/1/freq";
    const std::string_view other = "/synth/2/freq";
    const std::string_view malformedAddress = "/synth/1/freq/";

    const std::size_t before = oscpm_test::allocationCount();

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const oscpm::ParseResult rejected = oscpm::Pattern::parse("/no/trailing/");
    const oscpm::MatchResult matched = parsed.pattern().matches(address);
    const oscpm::MatchResult unmatched = parsed.pattern().matches(other);
    const oscpm::MatchResult malformed = parsed.pattern().matches(malformedAddress);
    const oscpm::MatchResult convenience = oscpm::match(pattern, address);
    const std::optional<oscpm::Error> checked = oscpm::validateAddress(malformedAddress);

    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed.ok());
    CHECK_FALSE(rejected.ok());
    CHECK(matched == oscpm::MatchResult::Match);
    CHECK(unmatched == oscpm::MatchResult::NoMatch);
    CHECK(malformed == oscpm::MatchResult::Malformed);
    CHECK(convenience == oscpm::MatchResult::Match);
    CHECK(checked.has_value());
}

TEST_CASE("allocation counter observes the heap", "[realtime]")
{
    const std::size_t before = oscpm_test::allocationCount();
    auto* p = new int(1);
    CHECK(oscpm_test::allocationCount() == before + 1);
    delete p;
}

TEST_CASE("ParseResult exposes the Pattern's text and the Error", "[pattern]")
{
    const oscpm::ParseResult ok = oscpm::Pattern::parse("/a/b");
    REQUIRE(ok);
    CHECK(ok.pattern().text() == "/a/b");

    const oscpm::ParseResult malformed = oscpm::Pattern::parse("/a/");
    REQUIRE_FALSE(malformed);
    CHECK(malformed.error().kind == oscpm::ErrorKind::TrailingSlash);
    CHECK(malformed.error().offset == 2);
    CHECK(std::string_view(oscpm::toString(malformed.error().kind)) == "TrailingSlash");
}

TEST_CASE("a Pattern outlives the ParseResult and copies freely", "[pattern]")
{
    oscpm::Pattern copy = oscpm::Pattern::parse("/a").pattern();
    const oscpm::Pattern second = copy;
    copy = second;
    CHECK(copy.matches("/a") == oscpm::MatchResult::Match);
    CHECK(second.matches("/b") == oscpm::MatchResult::NoMatch);
}
