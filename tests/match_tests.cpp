/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using oscpm::match;

TEST_CASE("a pattern without special characters matches only itself")
{
    STATIC_CHECK(match("/a", "/a"));
    STATIC_CHECK(match("/a/b", "/a/b"));
    STATIC_CHECK(match("/synth/1/freq", "/synth/1/freq"));
    STATIC_CHECK_FALSE(match("/a", "/A"));
    STATIC_CHECK_FALSE(match("/a", "/ab"));
    STATIC_CHECK_FALSE(match("/ab", "/a"));
    STATIC_CHECK_FALSE(match("/a/b", "/a"));
    STATIC_CHECK_FALSE(match("/a", "/a/b"));
}

TEST_CASE("pattern and address must have the same number of parts")
{
    STATIC_CHECK_FALSE(match("/a/b", "a/b"));
    STATIC_CHECK_FALSE(match("/a", "a"));
    STATIC_CHECK(match("/", "/"));
    STATIC_CHECK_FALSE(match("/", ""));
    STATIC_CHECK_FALSE(match("/", "/a"));
    STATIC_CHECK_FALSE(match("/a", "/"));
}

TEST_CASE("? matches exactly one byte within a part")
{
    STATIC_CHECK(match("/x?y", "/x1y"));
    STATIC_CHECK(match("/x?y", "/x?y"));
    STATIC_CHECK_FALSE(match("/x?y", "/xy"));
    STATIC_CHECK_FALSE(match("/x?y", "/x12y"));
    STATIC_CHECK_FALSE(match("/a?c", "/a/c"));
    STATIC_CHECK(match("/?/?", "/a/b"));
    STATIC_CHECK_FALSE(match("/?/*", "/xy/z"));
}

TEST_CASE("* matches zero or more bytes within a part")
{
    STATIC_CHECK(match("/x*y", "/xy"));
    STATIC_CHECK(match("/x*y", "/x123y"));
    STATIC_CHECK_FALSE(match("/x*z", "/x123y"));
    STATIC_CHECK(match("/*", "/"));
    STATIC_CHECK(match("/*", "/a"));
    STATIC_CHECK_FALSE(match("/*", "/a/b"));
    STATIC_CHECK_FALSE(match("/a*c", "/a/c"));
    STATIC_CHECK_FALSE(match("/a*", "/a/b"));
    STATIC_CHECK_FALSE(match("/a/*", "/a/b/c"));
    STATIC_CHECK_FALSE(match("/*/c", "/a/b/c"));
    STATIC_CHECK(match("/*/?", "/xy/z"));
    STATIC_CHECK(match("/**", "/abc"));
    STATIC_CHECK(match("/a*b*c", "/abc"));
    STATIC_CHECK(match("/a*b*c", "/axxbyyc"));
    STATIC_CHECK_FALSE(match("/a*b*c", "/axxbyy"));
    STATIC_CHECK(match("/*a", "/ba"));
    STATIC_CHECK(match("/*a", "/baba"));
    STATIC_CHECK_FALSE(match("/*a", "/bab"));
}

TEST_CASE("a set matches any one of its members")
{
    STATIC_CHECK(match("/x[321]y", "/x1y"));
    STATIC_CHECK_FALSE(match("/x[321]y", "/x4y"));
    STATIC_CHECK_FALSE(match("/x[321]y", "/xy"));
    STATIC_CHECK_FALSE(match("/x[321]y", "/x12y"));
}

TEST_CASE("two members separated by a minus are an inclusive ASCII range")
{
    STATIC_CHECK(match("/x[1-3]y", "/x1y"));
    STATIC_CHECK(match("/x[1-3]y", "/x2y"));
    STATIC_CHECK(match("/x[1-3]y", "/x3y"));
    STATIC_CHECK_FALSE(match("/x[1-3]y", "/x4y"));
    STATIC_CHECK(match("/x[a-z]y", "/xby"));
    STATIC_CHECK_FALSE(match("/x[a-z]y", "/x1y"));
    STATIC_CHECK(match("/[a-c-e]", "/b"));
    STATIC_CHECK(match("/[a-c-e]", "/-"));
    STATIC_CHECK(match("/[a-c-e]", "/e"));
    STATIC_CHECK_FALSE(match("/[a-c-e]", "/d"));
}

TEST_CASE("a reversed range matches nothing")
{
    STATIC_CHECK_FALSE(match("/[z-a]", "/a"));
    STATIC_CHECK_FALSE(match("/[z-a]", "/z"));
    STATIC_CHECK_FALSE(match("/[z-a]", "/m"));
    STATIC_CHECK(match("/x[3-2z]y", "/xzy"));
}

TEST_CASE("a minus at the start or end of a set is a member")
{
    STATIC_CHECK(match("/[-a]", "/-"));
    STATIC_CHECK(match("/[-a]", "/a"));
    STATIC_CHECK_FALSE(match("/[-a]", "/b"));
    STATIC_CHECK(match("/[a-]", "/-"));
    STATIC_CHECK(match("/[a-]", "/a"));
    STATIC_CHECK_FALSE(match("/[a-]", "/b"));
    STATIC_CHECK(match("/x[23-]y", "/x-y"));
}

TEST_CASE("an exclamation mark first in a set negates it, elsewhere it is a member")
{
    STATIC_CHECK(match("/x[!a-z]y", "/x1y"));
    STATIC_CHECK_FALSE(match("/x[!a-z]y", "/xby"));
    STATIC_CHECK(match("/x[a-z!]y", "/x!y"));
    STATIC_CHECK_FALSE(match("/[!-a]", "/-"));
    STATIC_CHECK_FALSE(match("/[!-a]", "/a"));
    STATIC_CHECK(match("/[!-a]", "/b"));
    STATIC_CHECK_FALSE(match("/x[!a]y", "/xy"));
}

TEST_CASE("an empty set matches nothing and a negated empty set matches any byte")
{
    STATIC_CHECK_FALSE(match("/[]", "/a"));
    STATIC_CHECK_FALSE(match("/[]", "/"));
    STATIC_CHECK_FALSE(match("/[]a]", "/]"));
    STATIC_CHECK_FALSE(match("/[]a]", "/a"));
    STATIC_CHECK_FALSE(match("/x[]a]", "/xa]"));
    STATIC_CHECK(match("/[!]", "/a"));
    STATIC_CHECK(match("/[!]", "/]"));
    STATIC_CHECK_FALSE(match("/[!]", "/"));
    STATIC_CHECK_FALSE(match("/[!]", "/ab"));
}

TEST_CASE("a set never matches a part separator")
{
    STATIC_CHECK_FALSE(match("/[/]", "//"));
    STATIC_CHECK_FALSE(match("/[.-0]", "//"));
    STATIC_CHECK(match("/[.-0]", "/."));
}

TEST_CASE("an unclosed set matches nothing")
{
    STATIC_CHECK_FALSE(match("/[abc", "/a"));
    STATIC_CHECK_FALSE(match("/[abc", "/[abc"));
    STATIC_CHECK_FALSE(match("/[a", "/a"));
    STATIC_CHECK_FALSE(match("/[", "/["));
    STATIC_CHECK_FALSE(match("/[!", "/a"));
}

TEST_CASE("a brace list matches exactly one of its alternatives")
{
    STATIC_CHECK(match("/x{1,10,11}y", "/x1y"));
    STATIC_CHECK(match("/x{1,10,11}y", "/x10y"));
    STATIC_CHECK(match("/x{1,10,11}y", "/x11y"));
    STATIC_CHECK_FALSE(match("/x{1,10,11}y", "/x2y"));
    STATIC_CHECK_FALSE(match("/x{1,10,11}y", "/x12y"));
    STATIC_CHECK_FALSE(match("/x{11}y", "/x12y"));
    STATIC_CHECK(match("/x{12}y", "/x12y"));
    STATIC_CHECK(match("/{a,b}", "/a"));
    STATIC_CHECK(match("/{a,b}", "/b"));
    STATIC_CHECK_FALSE(match("/{a,b}", "/ab"));
    STATIC_CHECK_FALSE(match("/{a,b}", "/c"));
    STATIC_CHECK(match("/{a,b}c", "/ac"));
    STATIC_CHECK(match("/{a,b}c", "/bc"));
    STATIC_CHECK_FALSE(match("/{a,b}c", "/c"));
}

TEST_CASE("every alternative of a brace list is tried")
{
    STATIC_CHECK(match("/{ab,a}c", "/ac"));
    STATIC_CHECK(match("/{a,ab}c", "/abc"));
    STATIC_CHECK(match("/*{ab,abc}d", "/xabcd"));
    STATIC_CHECK(match("/*{abc,ab}d", "/xabcd"));
    STATIC_CHECK(match("/{ab,a}{bcd,c}d", "/abcdd"));
    STATIC_CHECK(match("/{a,aa}{a,aa}{a,aa}b", "/aaaab"));
    STATIC_CHECK_FALSE(match("/{a,aa}{a,aa}{a,aa}b", "/aab"));
}

TEST_CASE("an empty alternative matches the empty string")
{
    STATIC_CHECK(match("/x{a,}", "/x"));
    STATIC_CHECK(match("/x{a,}", "/xa"));
    STATIC_CHECK_FALSE(match("/x{a,}", "/xb"));
    STATIC_CHECK(match("/x{,a}", "/x"));
    STATIC_CHECK(match("/x{,a}", "/xa"));
    STATIC_CHECK(match("/{}", "/"));
    STATIC_CHECK(match("/x{}", "/x"));
    STATIC_CHECK(match("/x{}y", "/xy"));
}

TEST_CASE("special characters inside a brace list are literal")
{
    STATIC_CHECK(match("/{a*,b}", "/a*"));
    STATIC_CHECK_FALSE(match("/{a*,b}", "/ab"));
    STATIC_CHECK_FALSE(match("/{a*,b}", "/axy"));
    STATIC_CHECK(match("/{a*,b}", "/b"));
    STATIC_CHECK(match("/{a?,b}", "/a?"));
    STATIC_CHECK_FALSE(match("/{a?,b}", "/ax"));
    STATIC_CHECK(match("/{[a],b}", "/[a]"));
    STATIC_CHECK_FALSE(match("/{[a],b}", "/a"));
}

TEST_CASE("a brace inside a brace list is literal and the first closing brace ends the list")
{
    STATIC_CHECK_FALSE(match("/{a,{b,c}}", "/a"));
    STATIC_CHECK_FALSE(match("/{a,{b,c}}", "/b"));
    STATIC_CHECK_FALSE(match("/{a,{b,c}}", "/c"));
    STATIC_CHECK_FALSE(match("/{a,{b,c}}", "/{b,c}"));
    STATIC_CHECK(match("/{a,{b,c}}", "/a}"));
    STATIC_CHECK(match("/{a,{b,c}}", "/{b}"));
    STATIC_CHECK(match("/{a,{b,c}}", "/c}"));
}

TEST_CASE("an unclosed brace list matches nothing")
{
    STATIC_CHECK_FALSE(match("/{a,b", "/a"));
    STATIC_CHECK_FALSE(match("/{a,b", "/{a,b"));
    STATIC_CHECK_FALSE(match("/{", "/{"));
}

TEST_CASE("a closing bracket or brace outside a construct is literal")
{
    STATIC_CHECK(match("/a]", "/a]"));
    STATIC_CHECK(match("/a}", "/a}"));
    STATIC_CHECK_FALSE(match("/a]", "/a"));
}

TEST_CASE("the // operator matches zero or more whole parts")
{
    STATIC_CHECK(match("//a", "/a"));
    STATIC_CHECK(match("//a", "/x/a"));
    STATIC_CHECK(match("//a", "/x/y/a"));
    STATIC_CHECK_FALSE(match("//a", "/xa"));
    STATIC_CHECK(match("//z", "/z"));
    STATIC_CHECK(match("//z", "/xy/z"));
    STATIC_CHECK(match("/a//c", "/a/c"));
    STATIC_CHECK(match("/a//c", "/a/b/c"));
    STATIC_CHECK(match("/a//c", "/a/b/d/c"));
    STATIC_CHECK(match("/a//c", "/a/c/c"));
    STATIC_CHECK_FALSE(match("/a//c", "/a/bc"));
    STATIC_CHECK_FALSE(match("/a//c", "/a/cc"));
    STATIC_CHECK_FALSE(match("/a//c", "/a"));
    STATIC_CHECK_FALSE(match("/a//c", "/c"));
}

TEST_CASE("the part before // must match whole")
{
    STATIC_CHECK_FALSE(match("/a//c", "/ab/c"));
    STATIC_CHECK_FALSE(match("/a//c", "/ba/c"));
}

TEST_CASE("a run of more than two slashes is one operator")
{
    STATIC_CHECK(match("/a///c", "/a/c"));
    STATIC_CHECK(match("/a///c", "/a/b/c"));
    STATIC_CHECK(match("/a////c", "/a/b/c"));
    STATIC_CHECK(match("///w/u", "/xy/z/w/u"));
    STATIC_CHECK_FALSE(match("///z/w", "/xy/z/w/u"));
}

TEST_CASE("a single trailing slash is an empty part")
{
    STATIC_CHECK_FALSE(match("/a/", "/a"));
    STATIC_CHECK(match("/a/", "/a/"));
    STATIC_CHECK_FALSE(match("/a/", "/a/b"));
    STATIC_CHECK_FALSE(match("//a//b/", "/a/b"));
}

TEST_CASE("a trailing run of slashes matches the part before it and every descendant")
{
    STATIC_CHECK(match("/a//", "/a"));
    STATIC_CHECK(match("/a//", "/a/b"));
    STATIC_CHECK(match("/a//", "/a/b/c"));
    STATIC_CHECK_FALSE(match("/a//", "/b"));
    STATIC_CHECK_FALSE(match("/a//", "/ab"));
    STATIC_CHECK(match("/a///", "/a/b"));
    STATIC_CHECK(match("/a/b//", "/a/b/c/d"));
    STATIC_CHECK_FALSE(match("/a/b//", "/a/c"));
    STATIC_CHECK(match("//a//", "/a"));
    STATIC_CHECK(match("//a//", "/x/a/y"));
    STATIC_CHECK_FALSE(match("//a//", "/x/b"));
}

TEST_CASE("a bare run of slashes matches every address")
{
    STATIC_CHECK(match("//", "/"));
    STATIC_CHECK(match("//", "/a"));
    STATIC_CHECK(match("//", "/a/b/c"));
    STATIC_CHECK(match("///", "/x/y"));
    STATIC_CHECK_FALSE(match("//", ""));
}

TEST_CASE("several // operators in one pattern")
{
    STATIC_CHECK(match("/a//b//c", "/a/b/c"));
    STATIC_CHECK(match("/a//b//c", "/a/x/b/y/c"));
    STATIC_CHECK_FALSE(match("/a//b//c", "/a/c"));
    STATIC_CHECK(match("/a//c/d", "/a/c/d"));
    STATIC_CHECK(match("/a//c/d", "/a/x/c/d"));
    STATIC_CHECK_FALSE(match("/a//c/d", "/a/c/x/d"));
    STATIC_CHECK(match("//a//a", "/a/a"));
    STATIC_CHECK(match("//a//a", "/x/a/y/a"));
    STATIC_CHECK(match("//a//a", "/a/a/a"));
    STATIC_CHECK_FALSE(match("//a//a", "/a"));
}

TEST_CASE("the // operator combines with wildcards in the parts around it")
{
    STATIC_CHECK(match("/a/*//c", "/a/b/c"));
    STATIC_CHECK_FALSE(match("/a/*//c", "/a/c"));
    STATIC_CHECK(match("/a//*/c", "/a/b/c"));
    STATIC_CHECK(match("/a//*", "/a/b/c"));
    STATIC_CHECK_FALSE(match("/a//*", "/a"));
    STATIC_CHECK(match("/[xyzw]/*/?", "/w/xy/z"));
    STATIC_CHECK(match("//{a,b}", "/x/b"));
}

TEST_CASE("matching is by byte")
{
    STATIC_CHECK(match("/caf\xc3\xa9", "/caf\xc3\xa9"));
    STATIC_CHECK_FALSE(match("/caf?", "/caf\xc3\xa9"));
    STATIC_CHECK(match("/caf??", "/caf\xc3\xa9"));
    STATIC_CHECK(match("/caf[\xc3]?", "/caf\xc3\xa9"));
    STATIC_CHECK(match("/[\x80-\xff]", "/\xc3"));
    STATIC_CHECK_FALSE(match("/[\x80-\xff]", "/a"));
}

TEST_CASE("an address part longer than the supported length never matches")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(match("/" + longest, "/" + longest));
    CHECK(match("/*", "/" + longest));
    CHECK_FALSE(match("/" + tooLong, "/" + tooLong));
    CHECK_FALSE(match("/*", "/" + tooLong));
}

namespace
{

template <typename Function>
double secondsTaken(Function&& function)
{
    const auto start = std::chrono::steady_clock::now();
    function();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

constexpr double kGenerousSeconds = 5.0;

}

TEST_CASE("a pattern with many stars completes in bounded time")
{
    const std::string address = "/" + std::string(200, 'a') + "c";
    std::string pattern = "/";
    for (int i = 0; i < 20; ++i)
    {
        pattern += "*a";
    }
    pattern += "*b";

    bool result = true;
    const double seconds = secondsTaken([&]
        { result = match(pattern, address); });
    CHECK_FALSE(result);
    CHECK(seconds < kGenerousSeconds);
}

TEST_CASE("a pattern with many brace lists completes in bounded time")
{
    const std::string address = "/" + std::string(200, 'a') + "c";
    std::string pattern = "/";
    for (int i = 0; i < 40; ++i)
    {
        pattern += "{a,aa}";
    }
    pattern += "b";

    bool result = true;
    const double seconds = secondsTaken([&]
        { result = match(pattern, address); });
    CHECK_FALSE(result);
    CHECK(seconds < kGenerousSeconds);
}

TEST_CASE("a brace list with thousands of empty members completes in bounded time")
{
    const std::string address = "/" + std::string(200, 'a') + "c";
    std::string pattern = "/{";
    for (int i = 0; i < 2000; ++i)
    {
        pattern += ",";
    }
    pattern += "}b";

    bool result = true;
    const double seconds = secondsTaken([&]
        { result = match(pattern, address); });
    CHECK_FALSE(result);
    CHECK(seconds < kGenerousSeconds);
}

TEST_CASE("a pattern with many // operators completes in bounded time")
{
    std::string address;
    for (int i = 0; i < 200; ++i)
    {
        address += "/a";
    }
    address += "/c";
    std::string pattern;
    for (int i = 0; i < 20; ++i)
    {
        pattern += "//a";
    }
    pattern += "//b";

    bool result = true;
    const double seconds = secondsTaken([&]
        { result = match(pattern, address); });
    CHECK_FALSE(result);
    CHECK(seconds < kGenerousSeconds);
}
