/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <doctest/doctest.h>

using oscpm::match;
using oscpm::Pattern;

TEST_CASE("a literal pattern matches only its own text")
{
    CHECK(match("/synth/1/freq", "/synth/1/freq"));
    CHECK_FALSE(match("/synth/1/freq", "/synth/1/amp"));
    CHECK_FALSE(match("/synth/1/freq", "/synth/1/freq/x"));
    CHECK_FALSE(match("/synth/1/freq", "/Synth/1/freq"));
    CHECK(match("/a.b+c", "/a.b+c"));
    CHECK_FALSE(match("/a.b+c", "/axbbc"));
}

TEST_CASE("wildcards never cross a slash")
{
    CHECK(match("/synth/*/freq", "/synth/12/freq"));
    CHECK_FALSE(match("/synth/*/freq", "/synth/1/osc/freq"));
    CHECK(match("/synth/1?/freq", "/synth/12/freq"));
    CHECK_FALSE(match("/synth/1?/freq", "/synth/1/freq"));
    CHECK_FALSE(match("/a*", "/a/b"));
}

TEST_CASE("character classes are the regex engine's")
{
    CHECK(match("/[1-3]", "/2"));
    CHECK_FALSE(match("/[1-3]", "/4"));
    CHECK(match("/[!x]", "/y"));
    CHECK_FALSE(match("/[!x]", "/x"));
    CHECK(match("/[abc]", "/b"));
}

TEST_CASE("brace lists are alternatives")
{
    CHECK(match("/{freq,amp}", "/amp"));
    CHECK_FALSE(match("/{freq,amp}", "/pan"));
    CHECK(match("/{a,}", "/"));
    CHECK(match("/x{a,}", "/x"));
}

TEST_CASE("the descendant operator matches zero or more whole parts")
{
    CHECK(match("//gain", "/gain"));
    CHECK(match("//gain", "/mixer/bus/3/gain"));
    CHECK(match("/a//c", "/a/c"));
    CHECK(match("/a//c", "/a/b/c"));
    CHECK_FALSE(match("/a//c", "/ab/c"));
    CHECK(match("/a/", "/a/b"));
}

TEST_CASE("a pattern the regex engine rejects is invalid and matches nothing")
{
    CHECK_FALSE(Pattern("/synth/[1").valid());
    CHECK_FALSE(Pattern("/synth/[1").matches("/synth/1"));
    CHECK_FALSE(match("/synth/[1", "/synth/1"));
    CHECK_FALSE(match("/[z-a]", "/m"));
    CHECK_FALSE(Pattern("/synth/{1").valid());
    CHECK_FALSE(match("/synth/{1", "/synth/1"));
}

TEST_CASE("a pattern built once matches many addresses")
{
    const Pattern pattern("/synth/*/{freq,amp}");
    REQUIRE(pattern.valid());
    CHECK(pattern.matches("/synth/1/freq"));
    CHECK(pattern.matches("/synth/22/amp"));
    CHECK_FALSE(pattern.matches("/synth/1/pan"));
    CHECK_FALSE(pattern.matches("synth/1/freq"));
    CHECK_FALSE(pattern.matches(""));
}
