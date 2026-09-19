/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using oscpm::isValidAddress;
using oscpm::isValidPattern;

TEST_CASE("a valid address starts with a slash and has non-empty parts")
{
    STATIC_CHECK(isValidAddress("/a"));
    STATIC_CHECK(isValidAddress("/synth/1/freq"));
    STATIC_CHECK(isValidAddress("/a-b_c.d~e!"));
    STATIC_CHECK_FALSE(isValidAddress(""));
    STATIC_CHECK_FALSE(isValidAddress("/"));
    STATIC_CHECK_FALSE(isValidAddress("a"));
    STATIC_CHECK_FALSE(isValidAddress("a/b"));
    STATIC_CHECK_FALSE(isValidAddress("/a/"));
    STATIC_CHECK_FALSE(isValidAddress("/a//b"));
    STATIC_CHECK_FALSE(isValidAddress("//a"));
}

TEST_CASE("an address contains no pattern characters, spaces or hashes")
{
    STATIC_CHECK_FALSE(isValidAddress("/a b"));
    STATIC_CHECK_FALSE(isValidAddress("/a#b"));
    STATIC_CHECK_FALSE(isValidAddress("/a*"));
    STATIC_CHECK_FALSE(isValidAddress("/a,b"));
    STATIC_CHECK_FALSE(isValidAddress("/a?"));
    STATIC_CHECK_FALSE(isValidAddress("/[a]"));
    STATIC_CHECK_FALSE(isValidAddress("/a]"));
    STATIC_CHECK_FALSE(isValidAddress("/{a}"));
    STATIC_CHECK_FALSE(isValidAddress("/a}"));
}

TEST_CASE("an address is printable ASCII")
{
    STATIC_CHECK_FALSE(isValidAddress("/a\tb"));
    STATIC_CHECK_FALSE(isValidAddress("/a\n"));
    STATIC_CHECK_FALSE(isValidAddress("/a\x7f"));
    STATIC_CHECK_FALSE(isValidAddress("/caf\xc3\xa9"));
    STATIC_CHECK(isValidAddress("/!\"$%&'()+-.0123456789:;<=>@ABCXYZ\\^_`abcxyz|~"));
}

TEST_CASE("an address part has a maximum length")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(isValidAddress("/" + longest));
    CHECK(isValidAddress("/" + longest + "/" + longest));
    CHECK_FALSE(isValidAddress("/" + tooLong));
    CHECK_FALSE(isValidAddress("/a/" + tooLong));
}

TEST_CASE("a valid pattern starts with a slash and is printable ASCII")
{
    STATIC_CHECK(isValidPattern("/a"));
    STATIC_CHECK(isValidPattern("/*"));
    STATIC_CHECK(isValidPattern("/a/?/[a-z]/{x,y}"));
    STATIC_CHECK_FALSE(isValidPattern(""));
    STATIC_CHECK_FALSE(isValidPattern("a"));
    STATIC_CHECK_FALSE(isValidPattern("a/b"));
    STATIC_CHECK_FALSE(isValidPattern("/a b"));
    STATIC_CHECK_FALSE(isValidPattern("/a\x7f"));
    STATIC_CHECK_FALSE(isValidPattern("/caf\xc3\xa9"));
}

TEST_CASE("a valid pattern may contain empty parts")
{
    STATIC_CHECK(isValidPattern("/"));
    STATIC_CHECK(isValidPattern("//a"));
    STATIC_CHECK(isValidPattern("/a//b"));
    STATIC_CHECK(isValidPattern("/a/"));
    STATIC_CHECK(isValidPattern("/a//"));
}

TEST_CASE("every set in a valid pattern is closed")
{
    STATIC_CHECK(isValidPattern("/[abc]"));
    STATIC_CHECK(isValidPattern("/[]"));
    STATIC_CHECK(isValidPattern("/[!]"));
    STATIC_CHECK(isValidPattern("/[[]"));
    STATIC_CHECK(isValidPattern("/[{]"));
    STATIC_CHECK(isValidPattern("/[*?,]"));
    STATIC_CHECK(isValidPattern("/[]a]"));
    STATIC_CHECK_FALSE(isValidPattern("/[abc"));
    STATIC_CHECK_FALSE(isValidPattern("/["));
    STATIC_CHECK_FALSE(isValidPattern("/[!"));
    STATIC_CHECK_FALSE(isValidPattern("/a/[b"));
}

TEST_CASE("a set or brace list closes within its own part")
{
    STATIC_CHECK(isValidPattern("/[a]/[b]"));
    STATIC_CHECK(isValidPattern("/{a}/{b}"));
    STATIC_CHECK_FALSE(isValidPattern("/[a/b]"));
    STATIC_CHECK_FALSE(isValidPattern("/{a/b}"));
    STATIC_CHECK_FALSE(isValidPattern("/{a,b}/[c"));
    STATIC_CHECK_FALSE(oscpm::match("/[a/b]", "/a/b"));
    STATIC_CHECK_FALSE(oscpm::match("/{a/b}", "/a/b"));
}

TEST_CASE("every brace list in a valid pattern is closed and not nested")
{
    STATIC_CHECK(isValidPattern("/{a,b}"));
    STATIC_CHECK(isValidPattern("/{}"));
    STATIC_CHECK(isValidPattern("/{a,}"));
    STATIC_CHECK(isValidPattern("/{a*,[b]}"));
    STATIC_CHECK_FALSE(isValidPattern("/{a,b"));
    STATIC_CHECK_FALSE(isValidPattern("/{"));
    STATIC_CHECK_FALSE(isValidPattern("/{a,{b,c}}"));
    STATIC_CHECK_FALSE(isValidPattern("/{a}/{b"));
}

TEST_CASE("a closing bracket or brace outside a construct is a valid literal")
{
    STATIC_CHECK(isValidPattern("/a]"));
    STATIC_CHECK(isValidPattern("/a}"));
    STATIC_CHECK(isValidPattern("/]"));
}

TEST_CASE("every valid address is a valid pattern that matches itself")
{
    STATIC_CHECK(isValidPattern("/synth/1/freq"));
    STATIC_CHECK(oscpm::match("/synth/1/freq", "/synth/1/freq"));
}
