#include <oscpm/pattern.hpp>

#include "allocation_counter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <new>
#include <optional>
#include <string>
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
    const std::string_view wildcards = "/synth/?[0-9]*/{freq,amp}";
    const std::string_view backtracking = "/*{a,ab}*b";
    const std::string_view descendant = "//synth//{freq,amp}";

    const std::size_t before = oscpm_test::allocationCount();

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const oscpm::ParseResult rejected = oscpm::Pattern::parse("/no/trailing/");
    const oscpm::ParseResult unterminated = oscpm::Pattern::parse("/{a,b");
    const oscpm::MatchResult matched = parsed.pattern().matches(address);
    const oscpm::MatchResult unmatched = parsed.pattern().matches(other);
    const oscpm::MatchResult malformed = parsed.pattern().matches(malformedAddress);
    const oscpm::MatchResult convenience = oscpm::match(pattern, address);
    const std::optional<oscpm::Error> checked = oscpm::validateAddress(malformedAddress);
    const oscpm::MatchResult wild = oscpm::match(wildcards, "/synth/12abc/amp");
    const oscpm::MatchResult backtracked = oscpm::match(backtracking, "/xxabxxb");
    const oscpm::MatchResult descended = oscpm::match(descendant, "/a/synth/1/osc/amp");
    const oscpm::ParseResult trailing = oscpm::Pattern::parse("/a//");

    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed.ok());
    CHECK_FALSE(rejected.ok());
    CHECK_FALSE(unterminated.ok());
    CHECK(matched == oscpm::MatchResult::Match);
    CHECK(unmatched == oscpm::MatchResult::NoMatch);
    CHECK(malformed == oscpm::MatchResult::Malformed);
    CHECK(convenience == oscpm::MatchResult::Match);
    CHECK(checked.has_value());
    CHECK(wild == oscpm::MatchResult::Match);
    CHECK(backtracked == oscpm::MatchResult::Match);
    CHECK(descended == oscpm::MatchResult::Match);
    CHECK_FALSE(trailing.ok());
}

// A constant expression cannot throw or allocate, so evaluating the matcher
// at compile time proves both for these paths.
TEST_CASE("parse and match are usable in constant expressions", "[pattern][realtime]")
{
    STATIC_REQUIRE(oscpm::match("/a*b", "/aXbYb") == oscpm::MatchResult::Match);
    STATIC_REQUIRE(oscpm::match("/ch[!0-4]", "/ch7") == oscpm::MatchResult::Match);
    STATIC_REQUIRE(oscpm::match("/{a,ab}{b,bb}", "/abbb") == oscpm::MatchResult::Match);
    STATIC_REQUIRE(oscpm::match("/synth/?", "/synth/12") == oscpm::MatchResult::NoMatch);
    STATIC_REQUIRE(oscpm::match("//a/b", "/a/x/a/b") == oscpm::MatchResult::Match);
    STATIC_REQUIRE(oscpm::match("/a//b//c", "/a/c/b") == oscpm::MatchResult::NoMatch);
    STATIC_REQUIRE(oscpm::Pattern::parse("/a///b").error().kind == oscpm::ErrorKind::EmptyPart);
    STATIC_REQUIRE(oscpm::Pattern::parse("/a///b").error().offset == 4);
    STATIC_REQUIRE(oscpm::match("/[a", "/a") == oscpm::MatchResult::Malformed);
    STATIC_REQUIRE(oscpm::Pattern::parse("/{a{b}").error().kind
                   == oscpm::ErrorKind::NestedAlternative);
    STATIC_REQUIRE(oscpm::Pattern::parse("/{a{b}").error().offset == 3);
}

// Matching is a set simulation with cost bounded by the product of the Part
// lengths (ADR 0004). These Patterns send a backtracking matcher exponential;
// the suite's own timeout is the bound being asserted.
TEST_CASE("Patterns that defeat a backtracking matcher still match", "[pattern][realtime]")
{
    const std::string longRun = "/" + std::string(100, 'a');

    std::string emptyMembers = "/";
    for (int i = 0; i < 2000; ++i) {
        emptyMembers += "{a,}";
    }
    const oscpm::ParseResult chained = oscpm::Pattern::parse(emptyMembers);
    REQUIRE(chained.ok());
    CHECK(chained.pattern().matches("/aaa") == oscpm::MatchResult::Match);
    CHECK(chained.pattern().matches("/aaab") == oscpm::MatchResult::NoMatch);
    CHECK(chained.pattern().matches(longRun) == oscpm::MatchResult::Match);

    std::string prefixMembers = "/";
    for (int i = 0; i < 1000; ++i) {
        prefixMembers += "{a,aa}";
    }
    const oscpm::ParseResult ambiguous = oscpm::Pattern::parse(prefixMembers);
    REQUIRE(ambiguous.ok());
    CHECK(ambiguous.pattern().matches("/" + std::string(1500, 'a')) == oscpm::MatchResult::Match);
    CHECK(ambiguous.pattern().matches("/" + std::string(1500, 'a') + "b")
          == oscpm::MatchResult::NoMatch);

    std::string stars = "/";
    for (int i = 0; i < 4000; ++i) {
        stars += "*a";
    }
    const oscpm::ParseResult starred = oscpm::Pattern::parse(stars);
    REQUIRE(starred.ok());
    CHECK(starred.pattern().matches("/aaa") == oscpm::MatchResult::NoMatch);
    CHECK(starred.pattern().matches("/" + std::string(4000, 'a')) == oscpm::MatchResult::Match);

    const std::string commas = "/*{" + std::string(oscpm::maxPatternPartLength - 3, ',') + "}";
    const oscpm::ParseResult manyEmptyMembers = oscpm::Pattern::parse(commas);
    REQUIRE(manyEmptyMembers.ok());
    CHECK(manyEmptyMembers.pattern().matches("/" + std::string(4000, 'a'))
          == oscpm::MatchResult::Match);

    // Across Parts, "//a//a//a..." against "/a/a/a.../a/b" makes a recursive
    // matcher try every way of distributing the Address Parts among the
    // operators before it can say NoMatch.
    std::string descendants;
    for (int i = 0; i < 200; ++i) {
        descendants += "//a";
    }
    std::string deep;
    for (int i = 0; i < 2000; ++i) {
        deep += "/a";
    }
    const oscpm::ParseResult descended = oscpm::Pattern::parse(descendants);
    REQUIRE(descended.ok());
    CHECK(descended.pattern().matches(deep) == oscpm::MatchResult::Match);
    CHECK(descended.pattern().matches(deep + "/b") == oscpm::MatchResult::NoMatch);
    CHECK(descended.pattern().matches("/a") == oscpm::MatchResult::NoMatch);
}

TEST_CASE("a Pattern Part may be at most maxPatternPartLength bytes", "[pattern]")
{
    const std::string maximal = std::string(oscpm::maxPatternPartLength, 'a');
    const std::string excessive = maximal + 'a';

    const std::string longestText = "/" + maximal; // Pattern views it (ADR 0003).
    const oscpm::ParseResult longest = oscpm::Pattern::parse(longestText);
    REQUIRE(longest.ok());
    CHECK(longest.pattern().matches("/" + maximal) == oscpm::MatchResult::Match);
    CHECK(longest.pattern().matches("/" + excessive) == oscpm::MatchResult::NoMatch);
    CHECK(oscpm::Pattern::parse("/" + maximal + "/" + maximal).ok());

    const oscpm::ParseResult tooLong = oscpm::Pattern::parse("/" + excessive);
    REQUIRE_FALSE(tooLong.ok());
    CHECK(tooLong.error().kind == oscpm::ErrorKind::PartTooLong);
    CHECK(tooLong.error().offset == 1 + oscpm::maxPatternPartLength);

    const oscpm::ParseResult second = oscpm::Pattern::parse("/ab/" + excessive + "/c");
    REQUIRE_FALSE(second.ok());
    CHECK(second.error().kind == oscpm::ErrorKind::PartTooLong);
    CHECK(second.error().offset == 4 + oscpm::maxPatternPartLength);

    // A wildcard Part is measured in bytes of Pattern text, not in matches.
    const oscpm::ParseResult star = oscpm::Pattern::parse("/*" + maximal);
    REQUIRE_FALSE(star.ok());
    CHECK(star.error().kind == oscpm::ErrorKind::PartTooLong);

    // The Address side has no limit.
    CHECK(oscpm::Pattern::parse("/*").pattern().matches("/" + excessive + excessive)
          == oscpm::MatchResult::Match);
    CHECK_FALSE(oscpm::validateAddress("/" + excessive).has_value());
}

TEST_CASE("the earliest fault wins between length and syntax", "[pattern]")
{
    const std::string excessive = std::string(oscpm::maxPatternPartLength + 1, 'a');

    const oscpm::ParseResult unterminated = oscpm::Pattern::parse("/[" + excessive);
    REQUIRE_FALSE(unterminated.ok());
    CHECK(unterminated.error().kind == oscpm::ErrorKind::UnterminatedCharacterClass);
    CHECK(unterminated.error().offset == 1);

    const oscpm::ParseResult reservedLate = oscpm::Pattern::parse("/" + excessive + "#");
    REQUIRE_FALSE(reservedLate.ok());
    CHECK(reservedLate.error().kind == oscpm::ErrorKind::PartTooLong);
    CHECK(reservedLate.error().offset == 1 + oscpm::maxPatternPartLength);

    const oscpm::ParseResult reservedEarly = oscpm::Pattern::parse("/#" + excessive);
    REQUIRE_FALSE(reservedEarly.ok());
    CHECK(reservedEarly.error().kind == oscpm::ErrorKind::IllegalCharacter);
    CHECK(reservedEarly.error().offset == 1);

    // At the same byte, the more specific fault is reported.
    const oscpm::ParseResult tie =
        oscpm::Pattern::parse("/" + std::string(oscpm::maxPatternPartLength, 'a') + "#");
    REQUIRE_FALSE(tie.ok());
    CHECK(tie.error().kind == oscpm::ErrorKind::IllegalCharacter);
    CHECK(tie.error().offset == 1 + oscpm::maxPatternPartLength);
}

TEST_CASE("allocation counter observes the heap", "[realtime]")
{
    // The allocation function is called directly: an optimising compiler
    // may elide a new-expression whose result is only deleted, but never a
    // direct call to operator new.
    const std::size_t before = oscpm_test::allocationCount();
    void* p = ::operator new(sizeof(int));
    CHECK(oscpm_test::allocationCount() == before + 1);
    ::operator delete(p);
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
