#include <oscpm/pattern.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using oscpm::Error;
using oscpm::ErrorCode;
using oscpm::match;
using oscpm::Pattern;
using oscpm::validate_address;
using oscpm::validate_pattern;

// The core matcher is constexpr, so patterns can be checked at compile time.
static_assert(match("/a/b", "/a/b"));
static_assert(!match("/a/b", "/a/c"));
static_assert(match("/a/*", "/a/xyz"));
static_assert(!match("/a/*", "/a/x/y"));
static_assert(match("/a//c", "/a/b/c"));
static_assert(!validate_pattern("/synth/{sine,saw}/freq[0-9]"));
static_assert(validate_pattern("/synth/[").code == ErrorCode::unterminated_bracket);
static_assert(!validate_address("/synth/1/freq"));
static_assert(validate_address("/synth/*").code == ErrorCode::reserved_character);

TEST_CASE("literal patterns match only themselves")
{
    CHECK(match("/a", "/a"));
    CHECK(match("/a/b/c", "/a/b/c"));
    CHECK_FALSE(match("/a", "/b"));
    CHECK_FALSE(match("/a", "/a/b"));
    CHECK_FALSE(match("/a/b", "/a"));
    CHECK_FALSE(match("/a", "/A"));
    CHECK_FALSE(match("/ab", "/a"));
    CHECK_FALSE(match("/a", "/ab"));
}

TEST_CASE("matching requires a leading slash on both sides")
{
    CHECK_FALSE(match("", ""));
    CHECK_FALSE(match("/a", ""));
    CHECK_FALSE(match("", "/a"));
    CHECK_FALSE(match("a", "a"));
    CHECK_FALSE(match("/a", "a"));
    CHECK_FALSE(match("a", "/a"));
}

TEST_CASE("'?' matches exactly one character within a part")
{
    CHECK(match("/?", "/a"));
    CHECK(match("/a?c", "/abc"));
    CHECK(match("/???", "/abc"));
    CHECK_FALSE(match("/?", "/"));
    CHECK_FALSE(match("/?", "/ab"));
    CHECK_FALSE(match("/a?", "/a"));
    CHECK_FALSE(match("/a?c", "/a/c"));
    CHECK_FALSE(match("/?", "/a/b"));
}

TEST_CASE("'*' matches any run of characters within a part")
{
    CHECK(match("/*", "/a"));
    CHECK(match("/*", "/abc"));
    CHECK(match("/a*", "/a"));
    CHECK(match("/a*", "/abc"));
    CHECK(match("/*c", "/abc"));
    CHECK(match("/*c", "/c"));
    CHECK(match("/a*c", "/ac"));
    CHECK(match("/a*c", "/abbbc"));
    CHECK(match("/*b*", "/abc"));
    CHECK(match("/**", "/abc"));
    CHECK(match("/*a*a*", "/banana"));
    CHECK_FALSE(match("/*a*a*a*a*", "/banana"));
    CHECK_FALSE(match("/a*c", "/ab"));
    CHECK_FALSE(match("/a*", "/b"));
}

TEST_CASE("'*' does not cross part boundaries")
{
    CHECK_FALSE(match("/*", "/a/b"));
    CHECK_FALSE(match("/a/*", "/a/b/c"));
    CHECK_FALSE(match("/*/c", "/a/b/c"));
    CHECK(match("/*/*", "/a/b"));
    CHECK(match("/a/*/c", "/a/b/c"));
    CHECK_FALSE(match("/a/*/c", "/a/c"));
}

TEST_CASE("'*' backtracks correctly with mixed wildcards")
{
    CHECK(match("/*?", "/abc"));
    CHECK(match("/?*", "/abc"));
    CHECK(match("/*?c", "/abc"));
    CHECK(match("/*[c]", "/abc"));
    CHECK(match("/*{bc,cd}", "/abc"));
    CHECK(match("/*{b,bc}", "/abc"));
    CHECK(match("/*{c,bc}x", "/abcx"));
    CHECK(match("/x*y*z", "/xaybyz"));
    CHECK_FALSE(match("/x*y*z", "/xaybya"));
    CHECK(match("/*b*c*", "/aabbcc"));
    CHECK(match("/a*b*c", "/abc"));
    CHECK(match("/a*b*c", "/aXbXc"));
    CHECK_FALSE(match("/a*b*c", "/acb"));
}

TEST_CASE("pathological '*' patterns stay fast")
{
    const std::string address = "/" + std::string(2000, 'a');
    CHECK_FALSE(match("/*a*a*a*a*a*a*a*a*a*a*a*b", address));
    CHECK(match("/*a*a*a*a*a*a*a*a*a*a*a*a", address));
}

TEST_CASE("'[...]' matches one character from a set")
{
    CHECK(match("/[abc]", "/a"));
    CHECK(match("/[abc]", "/c"));
    CHECK_FALSE(match("/[abc]", "/d"));
    CHECK_FALSE(match("/[abc]", "/ab"));
    CHECK_FALSE(match("/[abc]", "/"));
    CHECK(match("/x[abc]y", "/xby"));
    CHECK(match("/[a][b][c]", "/abc"));
}

TEST_CASE("'[...]' supports inclusive ranges")
{
    CHECK(match("/[a-c]", "/a"));
    CHECK(match("/[a-c]", "/b"));
    CHECK(match("/[a-c]", "/c"));
    CHECK_FALSE(match("/[a-c]", "/d"));
    CHECK(match("/[0-9]", "/5"));
    CHECK_FALSE(match("/[0-9]", "/a"));
    CHECK(match("/[a-cx-z]", "/y"));
    CHECK(match("/[a-cq]", "/q"));
    CHECK(match("/[qa-c]", "/q"));
    CHECK(match("/freq[0-9][0-9]", "/freq42"));
    CHECK_FALSE(match("/freq[0-9][0-9]", "/freq4"));
}

TEST_CASE("'-' at the start or end of a class is literal")
{
    CHECK(match("/[-a]", "/-"));
    CHECK(match("/[-a]", "/a"));
    CHECK(match("/[a-]", "/-"));
    CHECK(match("/[a-]", "/a"));
    CHECK_FALSE(match("/[a-]", "/b"));
    CHECK(match("/[a-c-]", "/-"));
    CHECK(match("/[!-]", "/a"));
    CHECK_FALSE(match("/[!-]", "/-"));
}

TEST_CASE("'!' at the start of a class negates it")
{
    CHECK(match("/[!abc]", "/d"));
    CHECK_FALSE(match("/[!abc]", "/a"));
    CHECK(match("/[!a-c]", "/d"));
    CHECK_FALSE(match("/[!a-c]", "/b"));
    CHECK_FALSE(match("/[!abc]", "/"));
    CHECK(match("/[!]", "/a"));
    CHECK(match("/[a!]", "/!"));
    CHECK(match("/[a!]", "/a"));
    CHECK_FALSE(match("/[a!]", "/b"));
}

TEST_CASE("'[...]' never matches '/'")
{
    CHECK_FALSE(match("/[!a]b", "//b"));
    CHECK_FALSE(match("/a[!x]b", "/a/b"));
}

TEST_CASE("'{...}' matches any of the listed strings")
{
    CHECK(match("/{foo,bar}", "/foo"));
    CHECK(match("/{foo,bar}", "/bar"));
    CHECK_FALSE(match("/{foo,bar}", "/baz"));
    CHECK_FALSE(match("/{foo,bar}", "/foobar"));
    CHECK(match("/{foo}", "/foo"));
    CHECK(match("/x{foo,bar}y", "/xfooy"));
    CHECK(match("/x{foo,bar}y", "/xbary"));
    CHECK_FALSE(match("/x{foo,bar}y", "/xbazy"));
    CHECK(match("/{a,ab}c", "/abc"));
    CHECK(match("/{ab,a}c", "/ac"));
    CHECK(match("/{a,b}{c,d}", "/ad"));
    CHECK_FALSE(match("/{a,b}{c,d}", "/ae"));
}

TEST_CASE("'{...}' allows empty alternatives")
{
    CHECK(match("/x{,y}", "/x"));
    CHECK(match("/x{,y}", "/xy"));
    CHECK(match("/x{y,}", "/x"));
    CHECK_FALSE(match("/x{,y}", "/xz"));
}

TEST_CASE("'{...}' contents are literal")
{
    CHECK(match("/{a?}", "/a?"));
    CHECK_FALSE(match("/{a?}", "/ab"));
    CHECK(match("/{a*}", "/a*"));
    CHECK_FALSE(match("/{a*}", "/abc"));
    CHECK(match("/{[a]}", "/[a]"));
}

TEST_CASE("OSC 1.1 '//' matches zero or more parts")
{
    CHECK(match("/a//b", "/a/b"));
    CHECK(match("/a//b", "/a/x/b"));
    CHECK(match("/a//b", "/a/x/y/b"));
    CHECK_FALSE(match("/a//b", "/a"));
    CHECK_FALSE(match("/a//b", "/a/b/c"));
    CHECK_FALSE(match("/a//b", "/b"));
    CHECK(match("//b", "/b"));
    CHECK(match("//b", "/a/b"));
    CHECK(match("//b", "/x/y/b"));
    CHECK_FALSE(match("//b", "/b/c"));
    CHECK(match("//*", "/a"));
    CHECK(match("//*", "/a/b/c"));
    CHECK(match("/a//*", "/a/b"));
    CHECK_FALSE(match("/a//*", "/a"));
    CHECK(match("/a//b//c", "/a/b/c"));
    CHECK(match("/a//b//c", "/a/1/b/2/3/c"));
    CHECK_FALSE(match("/a//b//c", "/a/1/c"));
    CHECK(match("//{freq,amp}", "/synth/1/freq"));
    CHECK(match("/synth//[0-9]", "/synth/voices/3"));
}

TEST_CASE("validate_pattern accepts well formed patterns")
{
    CHECK_FALSE(validate_pattern("/a"));
    CHECK_FALSE(validate_pattern("/a/b/c"));
    CHECK_FALSE(validate_pattern("/*"));
    CHECK_FALSE(validate_pattern("/a?/b*"));
    CHECK_FALSE(validate_pattern("/[abc]"));
    CHECK_FALSE(validate_pattern("/[a-z0-9]"));
    CHECK_FALSE(validate_pattern("/[!a-z]"));
    CHECK_FALSE(validate_pattern("/[-]"));
    CHECK_FALSE(validate_pattern("/[a-]"));
    CHECK_FALSE(validate_pattern("/[!]"));
    CHECK_FALSE(validate_pattern("/[]"));
    CHECK_FALSE(validate_pattern("/{a,b}"));
    CHECK_FALSE(validate_pattern("/{a,}"));
    CHECK_FALSE(validate_pattern("/{}"));
    CHECK_FALSE(validate_pattern("/a//b"));
    CHECK_FALSE(validate_pattern("//b"));
    CHECK_FALSE(validate_pattern("//*"));
    CHECK_FALSE(validate_pattern("/[*?{}]"));
    CHECK_FALSE(validate_pattern("/{[]?*}"));
}

TEST_CASE("validate_pattern reports errors with positions")
{
    CHECK(validate_pattern("") == Error{ErrorCode::empty, 0});
    CHECK(validate_pattern("a/b") == Error{ErrorCode::missing_leading_slash, 0});
    CHECK(validate_pattern("/") == Error{ErrorCode::empty_part, 1});
    CHECK(validate_pattern("/a/") == Error{ErrorCode::empty_part, 3});
    CHECK(validate_pattern("/a//") == Error{ErrorCode::empty_part, 4});
    CHECK(validate_pattern("/a///b") == Error{ErrorCode::empty_part, 4});
    CHECK(validate_pattern("/a[") == Error{ErrorCode::unterminated_bracket, 2});
    CHECK(validate_pattern("/a[bc") == Error{ErrorCode::unterminated_bracket, 2});
    CHECK(validate_pattern("/a[!") == Error{ErrorCode::unterminated_bracket, 2});
    CHECK(validate_pattern("/a{") == Error{ErrorCode::unterminated_brace, 2});
    CHECK(validate_pattern("/a{b,c") == Error{ErrorCode::unterminated_brace, 2});
    CHECK(validate_pattern("/{a{b}}") == Error{ErrorCode::nested_brace, 3});
    CHECK(validate_pattern("/a]") == Error{ErrorCode::unexpected_close_bracket, 2});
    CHECK(validate_pattern("/a}") == Error{ErrorCode::unexpected_close_brace, 2});
    CHECK(validate_pattern("/[a/b]") == Error{ErrorCode::slash_in_bracket, 3});
    CHECK(validate_pattern("/[a-/]") == Error{ErrorCode::slash_in_bracket, 4});
    CHECK(validate_pattern("/{a/b}") == Error{ErrorCode::slash_in_brace, 3});
    CHECK(validate_pattern("/[z-a]") == Error{ErrorCode::reversed_range, 2});
    CHECK(validate_pattern("/[!z-a]") == Error{ErrorCode::reversed_range, 3});
}

TEST_CASE("error messages are non-empty for every code")
{
    for (int c = 0; c <= static_cast<int>(ErrorCode::reserved_character); ++c) {
        const Error e{static_cast<ErrorCode>(c), 0};
        CHECK(std::string(e.message()).size() > 0);
    }
}

TEST_CASE("validate_address accepts literal addresses")
{
    CHECK_FALSE(validate_address("/a"));
    CHECK_FALSE(validate_address("/a/b/c"));
    CHECK_FALSE(validate_address("/synth/1/freq"));
    CHECK_FALSE(validate_address("/with-dash_and.dot"));
    CHECK_FALSE(validate_address("/unicode/ünïcödé"));
}

TEST_CASE("validate_address rejects malformed addresses")
{
    CHECK(validate_address("") == Error{ErrorCode::empty, 0});
    CHECK(validate_address("a") == Error{ErrorCode::missing_leading_slash, 0});
    CHECK(validate_address("/") == Error{ErrorCode::empty_part, 1});
    CHECK(validate_address("/a/") == Error{ErrorCode::empty_part, 3});
    CHECK(validate_address("/a//b") == Error{ErrorCode::empty_part, 3});
    CHECK(validate_address("/a b") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/a#b") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/a*") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/a,b") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/a?") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/[a]") == Error{ErrorCode::reserved_character, 1});
    CHECK(validate_address("/a]") == Error{ErrorCode::reserved_character, 2});
    CHECK(validate_address("/{a}") == Error{ErrorCode::reserved_character, 1});
    CHECK(validate_address("/a}") == Error{ErrorCode::reserved_character, 2});
}

TEST_CASE("literal_prefix and is_literal")
{
    CHECK(oscpm::literal_prefix("/a/b") == "/a/b");
    CHECK(oscpm::literal_prefix("/a/*") == "/a/");
    CHECK(oscpm::literal_prefix("/a/b?") == "/a/b");
    CHECK(oscpm::literal_prefix("/a[bc]") == "/a");
    CHECK(oscpm::literal_prefix("/a{b,c}") == "/a");
    CHECK(oscpm::literal_prefix("/a//b") == "/a/");
    CHECK(oscpm::literal_prefix("//b") == "/");
    CHECK(oscpm::literal_prefix("/a/b//*") == "/a/b/");
    CHECK(oscpm::literal_prefix("/a*//b") == "/a");
    CHECK(oscpm::literal_prefix("") == "");

    CHECK(oscpm::is_literal("/a/b"));
    CHECK(oscpm::is_literal("/"));
    CHECK_FALSE(oscpm::is_literal("/a/*"));
    CHECK_FALSE(oscpm::is_literal("/a?"));
    CHECK_FALSE(oscpm::is_literal("/[a]"));
    CHECK_FALSE(oscpm::is_literal("/{a}"));
    CHECK_FALSE(oscpm::is_literal("/a//b"));
}

TEST_CASE("Pattern::compile returns nullopt for malformed patterns")
{
    CHECK(Pattern::compile("/a/*").has_value());
    CHECK_FALSE(Pattern::compile("").has_value());
    CHECK_FALSE(Pattern::compile("/a[").has_value());
    CHECK_FALSE(Pattern::compile("/a/").has_value());
}

TEST_CASE("Pattern constructor throws PatternError with details")
{
    CHECK_NOTHROW(Pattern("/a/{b,c}"));
    try {
        Pattern p("/a/[x");
        FAIL("expected PatternError");
    } catch (const oscpm::PatternError& e) {
        CHECK(e.error().code == ErrorCode::unterminated_bracket);
        CHECK(e.error().position == 3);
        CHECK(std::string(e.what()).find("/a/[x") != std::string::npos);
        CHECK(std::string(e.what()).find("offset 3") != std::string::npos);
    }
}

TEST_CASE("Pattern matches like the free function")
{
    const Pattern p("/synth/*/{freq,amp}");
    CHECK(p.matches("/synth/1/freq"));
    CHECK(p.matches("/synth/voice/amp"));
    CHECK_FALSE(p.matches("/synth/1/pan"));
    CHECK_FALSE(p.matches("/synth/1/2/freq"));
    CHECK(match(p, "/synth/1/freq"));
    CHECK(p.str() == "/synth/*/{freq,amp}");
    CHECK_FALSE(p.is_literal());
    CHECK(p.literal_prefix() == "/synth/");
}

TEST_CASE("literal Pattern takes the equality path")
{
    const Pattern p("/a/b");
    CHECK(p.is_literal());
    CHECK(p.literal_prefix() == "/a/b");
    CHECK(p.matches("/a/b"));
    CHECK_FALSE(p.matches("/a/bc"));
    CHECK_FALSE(p.matches("/a"));
    CHECK_FALSE(p.matches(""));
}

TEST_CASE("Pattern equality and copying")
{
    const Pattern a("/x/*");
    const Pattern b = a;
    CHECK(a == b);
    CHECK(a != Pattern("/x/?"));
    CHECK(b.matches("/x/anything"));
}
