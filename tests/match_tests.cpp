/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm_regex/oscpm_regex.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using oscpm_regex::Error;
using oscpm_regex::match;
using oscpm_regex::Pattern;
using oscpm_regex::validateAddress;

TEST_CASE("a literal pattern matches only its own text")
{
    CHECK(match("/synth/1/freq", "/synth/1/freq"));
    CHECK_FALSE(match("/synth/1/freq", "/synth/1/amp"));
    CHECK_FALSE(match("/synth/1/freq", "/synth/1/freq/x"));
    CHECK_FALSE(match("/synth/1/freq", "/Synth/1/freq"));
}

TEST_CASE("wildcards never cross a slash")
{
    CHECK(match("/synth/*/freq", "/synth/12/freq"));
    CHECK(match("/synth/*/freq", "/synth//freq"));
    CHECK_FALSE(match("/synth/*/freq", "/synth/1/osc/freq"));
    CHECK(match("/synth/1?/freq", "/synth/12/freq"));
    CHECK_FALSE(match("/synth/1?/freq", "/synth/1/freq"));
    CHECK_FALSE(match("/a*", "/a/b"));
}

TEST_CASE("character classes follow glob rules")
{
    CHECK(match("/[1-3]", "/2"));
    CHECK_FALSE(match("/[1-3]", "/4"));
    CHECK(match("/[!x]", "/y"));
    CHECK_FALSE(match("/[!x]", "/x"));
    CHECK(match("/[--a]", "/."));
    CHECK_FALSE(match("/[z-a]", "/m"));
    CHECK_FALSE(match("/[]", "/a"));
    CHECK(match("/[!]", "/a"));
}

TEST_CASE("brace lists are literal alternatives")
{
    CHECK(match("/{freq,amp}", "/amp"));
    CHECK_FALSE(match("/{freq,amp}", "/pan"));
    CHECK(match("/{a,}", "/"));
    CHECK(match("/x{a,}", "/x"));
    CHECK(match("/{a*,b}", "/a*"));
}

TEST_CASE("the descendant operator matches zero or more whole parts")
{
    CHECK(match("//gain", "/gain"));
    CHECK(match("//gain", "/mixer/bus/3/gain"));
    CHECK(match("/a//c", "/a/c"));
    CHECK(match("/a//c", "/a/b/c"));
    CHECK_FALSE(match("/a//c", "/ab/c"));
    CHECK(match("/a///c", "/a/b/c"));
    CHECK(match("/a/", "/a/b"));
    CHECK(match("/a/", "/a"));
}

TEST_CASE("a malformed pattern reports why and matches nothing")
{
    CHECK_FALSE(Pattern("/synth").error());
    CHECK(Pattern("synth").error() == Error::MissingLeadingSlash);
    CHECK(Pattern("/synth/[1").error() == Error::UnterminatedClass);
    CHECK(Pattern("/synth/{1").error() == Error::UnterminatedBraces);
    CHECK(Pattern("/{a,{b}}").error() == Error::NestedBraces);
    CHECK(Pattern("/a b").error() == Error::IllegalByte);
    CHECK(Pattern(std::string("/\xC3\xA9")).error() == Error::IllegalByte);
    CHECK_FALSE(Pattern("/synth/[1").valid());
    CHECK_FALSE(Pattern("/synth/[1").matches("/synth/1"));
    CHECK_FALSE(match("/synth/[1", "/synth/1"));
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

TEST_CASE("validateAddress reports the first fault in an address")
{
    CHECK_FALSE(validateAddress("/a"));
    CHECK_FALSE(validateAddress("/synth/1/freq"));
    CHECK(validateAddress("") == Error::MissingLeadingSlash);
    CHECK(validateAddress("a") == Error::MissingLeadingSlash);
    CHECK(validateAddress("/") == Error::TrailingSlash);
    CHECK(validateAddress("/a/") == Error::TrailingSlash);
    CHECK(validateAddress("/a//b") == Error::EmptyPart);
    CHECK(validateAddress("/a b") == Error::IllegalByte);
    CHECK(validateAddress("/a*") == Error::IllegalByte);
    CHECK(validateAddress("/#bundle") == Error::IllegalByte);
}
