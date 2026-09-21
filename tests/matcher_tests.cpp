/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm_regex/oscpm_regex.h>

#include <catch2/catch_test_macros.hpp>

using oscpm_regex::Error;
using oscpm_regex::Matcher;

TEST_CASE("a matcher memoises each pattern and address pair once")
{
    Matcher matcher;
    CHECK(matcher.match("/synth/*/freq", "/synth/1/freq"));
    CHECK(matcher.match("/synth/*/freq", "/synth/1/freq"));
    CHECK(matcher.size() == 1);
    CHECK_FALSE(matcher.match("/synth/*/freq", "/synth/1/amp"));
    CHECK(matcher.size() == 2);
    CHECK(matcher.match("//freq", "/synth/1/freq"));
    CHECK(matcher.size() == 3);
}

TEST_CASE("a matcher compiles a pattern once and reports its fault")
{
    Matcher matcher;
    CHECK(matcher.compiled("/synth/[1-3]/{freq,amp}").valid());
    CHECK(&matcher.compiled("/synth/[1-3]/{freq,amp}") == &matcher.compiled("/synth/[1-3]/{freq,amp}"));
    CHECK(matcher.compiled("/synth/[1").error() == Error::UnterminatedClass);
    CHECK_FALSE(matcher.match("/synth/[1", "/synth/1"));
}

TEST_CASE("clear forgets every verdict and pattern")
{
    Matcher matcher;
    matcher.match("/a", "/a");
    matcher.clear();
    CHECK(matcher.size() == 0);
    CHECK(matcher.match("/a", "/a"));
}
