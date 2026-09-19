#include <oscpm/oscpm.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstring>
#include <string>

using oscpm::Error;
using oscpm::match;
using oscpm::Pattern;
using oscpm::validate;

// The whole matching path is constexpr, so a handful of cases are checked
// at compile time as well.
static_assert(match("/a/b", "/a/b"));
static_assert(match("/a/*", "/a/b"));
static_assert(match("//b", "/a/b"));
static_assert(!match("/a/*", "/a/b/c"));
static_assert(validate("/a[") == Error::UnterminatedCharacterClass);

TEST_CASE("literal patterns match only their own text")
{
    CHECK(match("/synth/freq", "/synth/freq"));
    CHECK_FALSE(match("/synth/freq", "/synth/freqs"));
    CHECK_FALSE(match("/synth/freq", "/synth/fre"));
    CHECK_FALSE(match("/synth/freq", "/synth/Freq"));
    CHECK_FALSE(match("/synth", "/synth/freq"));
    CHECK_FALSE(match("/synth/freq", "/synth"));
}

TEST_CASE("'?' matches exactly one character that is not '/'")
{
    CHECK(match("/a?c", "/abc"));
    CHECK(match("/a?c", "/a?c"));
    CHECK_FALSE(match("/a?c", "/ac"));
    CHECK_FALSE(match("/a?c", "/abbc"));
    CHECK_FALSE(match("/a?c", "/a/c"));
    CHECK(match("/???", "/abc"));
}

TEST_CASE("'*' matches any run of characters within a single part")
{
    CHECK(match("/*", "/"));
    CHECK(match("/*", "/anything"));
    CHECK(match("/a*", "/a"));
    CHECK(match("/a*", "/abc"));
    CHECK(match("/*c", "/abc"));
    CHECK(match("/a*c", "/ac"));
    CHECK(match("/a*c", "/abbbc"));
    CHECK(match("/a*b*c", "/aXbYc"));
    CHECK(match("/a**c", "/abc"));
    CHECK(match("/*/*", "/a/b"));
    CHECK(match("/a*/b", "/ab/b"));
    CHECK(match("/*/x", "/anything/x"));
}

TEST_CASE("'*' never crosses a part boundary")
{
    CHECK_FALSE(match("/*", "/a/b"));
    CHECK_FALSE(match("/a*", "/a/b"));
    CHECK_FALSE(match("/*b", "/a/b"));
    CHECK_FALSE(match("/a/*", "/a/b/c"));
    CHECK_FALSE(match("/a/*", "/a"));
}

TEST_CASE("character classes")
{
    SECTION("list of characters")
    {
        CHECK(match("/[abc]", "/a"));
        CHECK(match("/[abc]", "/c"));
        CHECK_FALSE(match("/[abc]", "/d"));
        CHECK_FALSE(match("/[abc]", "/ab"));
        CHECK_FALSE(match("/[abc]", "/"));
    }
    SECTION("ranges")
    {
        CHECK(match("/[a-c]", "/b"));
        CHECK_FALSE(match("/[a-c]", "/d"));
        CHECK(match("/[0-9]", "/7"));
        CHECK(match("/ch[0-9]", "/ch3"));
        CHECK(match("/[a-cx-z]", "/y"));
        CHECK_FALSE(match("/[a-cx-z]", "/m"));
        CHECK(match("/[a-c5]", "/5"));
    }
    SECTION("negation with a leading '!'")
    {
        CHECK(match("/[!abc]", "/d"));
        CHECK_FALSE(match("/[!abc]", "/a"));
        CHECK(match("/[!a-c]", "/z"));
        CHECK_FALSE(match("/[!a-c]", "/b"));
    }
    SECTION("'!' after the first position is literal")
    {
        CHECK(match("/[a!]", "/!"));
        CHECK(match("/[a!]", "/a"));
        CHECK_FALSE(match("/[a!]", "/b"));
    }
    SECTION("'-' at the start or end is literal")
    {
        CHECK(match("/[-a]", "/-"));
        CHECK(match("/[a-]", "/-"));
        CHECK(match("/[!-]", "/x"));
        CHECK_FALSE(match("/[!-]", "/-"));
        CHECK(match("/[a-z-]", "/-"));
        CHECK(match("/[a-z-]", "/q"));
    }
    SECTION("a class never matches '/'")
    {
        CHECK_FALSE(match("/a[/]b", "/a/b"));   // rejected as malformed
        CHECK_FALSE(match("/a[!x]b", "/a/b"));  // well formed, but '/' is excluded
    }
    SECTION("other operators are ordinary characters inside a class")
    {
        CHECK(match("/[*?]", "/*"));
        CHECK(match("/[*?]", "/?"));
        CHECK_FALSE(match("/[*?]", "/a"));
    }
}

TEST_CASE("alternatives")
{
    CHECK(match("/{foo,bar}", "/foo"));
    CHECK(match("/{foo,bar}", "/bar"));
    CHECK_FALSE(match("/{foo,bar}", "/baz"));
    CHECK_FALSE(match("/{foo,bar}", "/foobar"));
    CHECK(match("/{foo,bar}/x", "/bar/x"));
    CHECK(match("/x{foo,bar}y", "/xfooy"));
    CHECK(match("/{a,ab}c", "/abc"));
    CHECK(match("/{a,ab}c", "/ac"));
    CHECK(match("/{a}", "/a"));

    SECTION("an empty alternative makes the group optional")
    {
        CHECK(match("/x{a,}", "/xa"));
        CHECK(match("/x{a,}", "/x"));
        CHECK(match("/x{,a}", "/x"));
    }
    SECTION("alternatives are literal text")
    {
        CHECK(validate("/{a*,b}") == Error::UnexpectedCharacter);
        CHECK(validate("/{a?,b}") == Error::UnexpectedCharacter);
        CHECK(validate("/{[ab],c}") == Error::UnexpectedCharacter);
        CHECK(validate("/{a/b,c}") == Error::UnexpectedCharacter);
    }
}

TEST_CASE("'//' matches zero or more whole parts")
{
    CHECK(match("//foo", "/foo"));
    CHECK(match("//foo", "/a/foo"));
    CHECK(match("//foo", "/a/b/c/foo"));
    CHECK_FALSE(match("//foo", "/foo/bar"));
    CHECK_FALSE(match("//foo", "/xfoo"));

    CHECK(match("/a//b", "/a/b"));
    CHECK(match("/a//b", "/a/x/b"));
    CHECK(match("/a//b", "/a/x/y/b"));
    CHECK(match("/a//b", "/a/b/b"));
    CHECK_FALSE(match("/a//b", "/a/bx"));
    CHECK_FALSE(match("/a//b", "/ax/b"));
    CHECK_FALSE(match("/a//b", "/a/b/c"));

    SECTION("'//' begins only at a part boundary")
    {
        CHECK_FALSE(match("/a//b", "/ax/b"));
        CHECK(match("/a*//b", "/ax/b"));
        CHECK(match("/a*//b", "/ax/y/b"));
        CHECK_FALSE(match("/a?//b", "/a/b"));
    }

    CHECK(match("/a//b//c", "/a/b/c"));
    CHECK(match("/a//b//c", "/a/1/b/2/3/c"));
    CHECK_FALSE(match("/a//b//c", "/a/c/b"));

    CHECK(match("//*", "/a/b/c"));
    CHECK(match("//*/x", "/a/b/x"));
    CHECK(match("//{foo,bar}", "/x/y/bar"));
    CHECK(match("//[a-c]", "/x/b"));
}

TEST_CASE("combined operators")
{
    CHECK(match("/synth/[0-9]/{freq,amp}", "/synth/3/amp"));
    CHECK_FALSE(match("/synth/[0-9]/{freq,amp}", "/synth/x/amp"));
    CHECK(match("/synth/*/{freq,amp}", "/synth/anything/freq"));
    CHECK(match("//ch?/gain*", "/mixer/ch1/gain_db"));
}

TEST_CASE("validate accepts well-formed patterns")
{
    const char* pattern = GENERATE(
        "/", "/a", "/a/b", "/*", "/a/*/b", "/?", "/[a]", "/[!a]", "/[a-z]", "/[a-]", "/[-a]",
        "/{a}", "/{a,b}", "/{a,}", "//a", "/a//b", "/a b", "/a#b", "/a.b", "/[*?]");
    CAPTURE(pattern);
    // "/" alone is the one exception, tested below.
    if (std::strcmp(pattern, "/") == 0) return;
    CHECK(validate(pattern) == Error::None);
}

TEST_CASE("validate reports why a pattern is malformed")
{
    CHECK(validate("") == Error::Empty);
    CHECK(validate("a/b") == Error::MissingLeadingSlash);
    CHECK(validate("a") == Error::MissingLeadingSlash);

    CHECK(validate("/") == Error::EmptyPart);
    CHECK(validate("/a/") == Error::EmptyPart);
    CHECK(validate("/a//") == Error::EmptyPart);
    CHECK(validate("///a") == Error::EmptyPart);
    CHECK(validate("/a///b") == Error::EmptyPart);

    CHECK(validate("/[") == Error::UnterminatedCharacterClass);
    CHECK(validate("/[a") == Error::UnterminatedCharacterClass);
    CHECK(validate("/[!") == Error::UnterminatedCharacterClass);
    CHECK(validate("/[a-z") == Error::UnterminatedCharacterClass);
    CHECK(validate("/[]") == Error::EmptyCharacterClass);
    CHECK(validate("/[!]") == Error::EmptyCharacterClass);
    CHECK(validate("/[z-a]") == Error::InvalidRange);
    CHECK(validate("/[a[b]") == Error::UnexpectedCharacter);
    CHECK(validate("/[a/b]") == Error::UnexpectedCharacter);

    CHECK(validate("/{") == Error::UnterminatedAlternatives);
    CHECK(validate("/{a,b") == Error::UnterminatedAlternatives);
    CHECK(validate("/{}") == Error::EmptyAlternatives);
    CHECK(validate("/{a,{b}}") == Error::UnexpectedCharacter);

    CHECK(validate("/a]") == Error::UnexpectedCharacter);
    CHECK(validate("/a}") == Error::UnexpectedCharacter);
    CHECK(validate("/a,b") == Error::UnexpectedCharacter);
    CHECK(validate("/a\tb") == Error::UnexpectedCharacter);
    CHECK(validate("/a\nb") == Error::UnexpectedCharacter);
    CHECK(validate("/caf\xc3\xa9") == Error::UnexpectedCharacter);
}

TEST_CASE("a malformed pattern never matches")
{
    CHECK_FALSE(match("", ""));
    CHECK_FALSE(match("", "/a"));
    CHECK_FALSE(match("a", "a"));
    CHECK_FALSE(match("/a/", "/a/"));
    CHECK_FALSE(match("/[", "/["));
    CHECK_FALSE(match("/{a", "/{a"));
    CHECK_FALSE(match("/a,b", "/a,b"));
}

TEST_CASE("an address must begin with '/'")
{
    CHECK_FALSE(match("/*", ""));
    CHECK_FALSE(match("/*", "a"));
    CHECK_FALSE(match("//*", "a/b"));
    CHECK(match("/*", "/"));
}

TEST_CASE("addresses may contain characters reserved in patterns")
{
    CHECK(match("/*", "/a*b"));
    CHECK(match("/?", "/?"));
    CHECK(match("/a{b,c}", "/a{b,c}") == false); // pattern operator, not literal text
    CHECK(match("/*", "/a{b,c}"));
    CHECK(match("/*", "/[x]"));
}

TEST_CASE("errorMessage covers every error")
{
    const Error error = GENERATE(
        Error::None, Error::Empty, Error::MissingLeadingSlash, Error::EmptyPart,
        Error::UnterminatedCharacterClass, Error::EmptyCharacterClass, Error::InvalidRange,
        Error::UnterminatedAlternatives, Error::EmptyAlternatives, Error::UnexpectedCharacter);
    CAPTURE(static_cast<int>(error));
    CHECK(oscpm::errorMessage(error) != nullptr);
    CHECK(std::string(oscpm::errorMessage(error)) != "unknown error");
}

TEST_CASE("Pattern validates once and matches many times")
{
    const Pattern p("/synth/[0-9]/{freq,amp}");
    REQUIRE(p.valid());
    REQUIRE(static_cast<bool>(p));
    CHECK(p.error() == Error::None);
    CHECK(p.str() == "/synth/[0-9]/{freq,amp}");
    CHECK_FALSE(p.isLiteral());
    CHECK(p.matches("/synth/1/freq"));
    CHECK(p.matches("/synth/9/amp"));
    CHECK_FALSE(p.matches("/synth/10/amp"));
    CHECK_FALSE(p.matches("synth/1/freq"));
}

TEST_CASE("Pattern reports errors and never matches when invalid")
{
    const Pattern p("/synth/[0-9");
    CHECK_FALSE(p.valid());
    CHECK_FALSE(static_cast<bool>(p));
    CHECK(p.error() == Error::UnterminatedCharacterClass);
    CHECK(p.str() == "/synth/[0-9");
    CHECK_FALSE(p.isLiteral());
    CHECK_FALSE(p.matches("/synth/[0-9"));
    CHECK_FALSE(p.matches("/synth/1"));
}

TEST_CASE("a default-constructed Pattern is invalid")
{
    const Pattern p;
    CHECK_FALSE(p.valid());
    CHECK(p.error() == Error::Empty);
    CHECK(p.str().empty());
    CHECK_FALSE(p.matches("/"));
}

TEST_CASE("Pattern::isLiteral is true only when there are no operators")
{
    CHECK(Pattern("/synth/freq").isLiteral());
    CHECK(Pattern("/a b#c").isLiteral());
    CHECK_FALSE(Pattern("/synth/*").isLiteral());
    CHECK_FALSE(Pattern("/synth/?").isLiteral());
    CHECK_FALSE(Pattern("/synth/[a]").isLiteral());
    CHECK_FALSE(Pattern("/synth/{a}").isLiteral());
    CHECK_FALSE(Pattern("//freq").isLiteral());
    CHECK_FALSE(Pattern("/a/").isLiteral());

    const Pattern literal("/synth/freq");
    CHECK(literal.matches("/synth/freq"));
    CHECK_FALSE(literal.matches("/synth/fre"));
    CHECK_FALSE(literal.matches("/synth/freq/"));
}

TEST_CASE("Pattern agrees with match")
{
    struct Case { const char* pattern; const char* address; };
    const Case c = GENERATE(
        Case{"/a/*", "/a/b"}, Case{"/a/*", "/a/b/c"}, Case{"//x", "/p/q/x"},
        Case{"/[!a]", "/a"}, Case{"/{a,b}c", "/bc"}, Case{"/bad[", "/bad["}, Case{"/a", "a"});
    CAPTURE(c.pattern, c.address);
    CHECK(Pattern(c.pattern).matches(c.address) == match(c.pattern, c.address));
}
