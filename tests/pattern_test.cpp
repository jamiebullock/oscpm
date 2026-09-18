#include <oscpm/pattern.hpp>

#include "allocation_counter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>
#include <type_traits>

TEST_CASE("Pattern is a trivially copyable non-owning view", "[pattern]")
{
    STATIC_REQUIRE(std::is_trivially_copyable_v<oscpm::Pattern>);
    STATIC_REQUIRE(sizeof(oscpm::Pattern) == sizeof(std::string_view));
}

TEST_CASE("parse and matches do not allocate", "[pattern][realtime]")
{
    const std::string_view pattern = "/synth/1/freq";
    const std::string_view address = "/synth/1/freq";
    const std::string_view other = "/synth/2/freq";
    const std::string_view bad = "/synth/1/freq/";

    const std::size_t before = oscpm_test::allocationCount();

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const oscpm::ParseResult rejected = oscpm::Pattern::parse("/no/trailing/");
    const oscpm::MatchResult hit = parsed.pattern().matches(address);
    const oscpm::MatchResult miss = parsed.pattern().matches(other);
    const oscpm::MatchResult malformed = parsed.pattern().matches(bad);
    const oscpm::MatchResult convenience = oscpm::match(pattern, address);
    const std::optional<oscpm::Error> checked = oscpm::validateAddress(bad);

    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed.ok());
    CHECK_FALSE(rejected.ok());
    CHECK(hit == oscpm::MatchResult::Match);
    CHECK(miss == oscpm::MatchResult::NoMatch);
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

    const oscpm::ParseResult bad = oscpm::Pattern::parse("/a/");
    REQUIRE_FALSE(bad);
    CHECK(bad.error().kind == oscpm::ErrorKind::TrailingSlash);
    CHECK(bad.error().offset == 2);
    CHECK(std::string_view(oscpm::name(bad.error().kind)) == "TrailingSlash");
}

TEST_CASE("a Pattern outlives the ParseResult and copies freely", "[pattern]")
{
    oscpm::Pattern copy = oscpm::Pattern::parse("/a").pattern();
    const oscpm::Pattern second = copy;
    copy = second;
    CHECK(copy.matches("/a") == oscpm::MatchResult::Match);
    CHECK(second.matches("/b") == oscpm::MatchResult::NoMatch);
}
