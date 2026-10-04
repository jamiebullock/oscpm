/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <oscpm/pattern.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <new>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

using oscpm::Error;
using oscpm::Pattern;

namespace
{

constexpr Pattern::ParseResult kWildcard = Pattern::parse("/synth/*/{freq,amp}");
constexpr Pattern::ParseResult kDescendant = Pattern::parse("//gain");
constexpr Pattern::ParseResult kUnterminated = Pattern::parse("/synth/[1-3");

static_assert(kWildcard);
static_assert(kWildcard.pattern().matches("/synth/12/amp"));
static_assert(!kWildcard.pattern().matches("/synth/12/gain"));
static_assert(kDescendant.pattern().matches("/mixer/bus/3/gain"));
static_assert(!kUnterminated);
static_assert(kUnterminated.error() == Error::UnterminatedClass);
static_assert(std::is_trivially_copyable_v<Pattern>);
static_assert(std::is_trivially_copyable_v<Pattern::ParseResult>);

constexpr bool faults(const Pattern::ParseResult& result, Error kind)
{
    return !result && result.error() == kind;
}

}

TEST_CASE("parse yields a pattern for a well-formed pattern")
{
    static_assert(Pattern::parse("/a"));
    static_assert(Pattern::parse("/"));
    static_assert(Pattern::parse("//"));
    static_assert(Pattern::parse("/a/?/[a-z]/{x,y}"));
    static_assert(Pattern::parse("/{a,{b,c}}"));
    static_assert(Pattern::parse("/{a,{b,c}}]} #\xc3\xa9"));
    static_assert(Pattern::parse("/a]"));
    static_assert(Pattern::parse("/caf\xc3\xa9"));
}

TEST_CASE("parse reports the first fault")
{
    static_assert(faults(Pattern::parse(""), Error::MissingLeadingSlash));
    static_assert(faults(Pattern::parse("a/b"), Error::MissingLeadingSlash));
    static_assert(faults(Pattern::parse("/a[b"), Error::UnterminatedClass));
    static_assert(faults(Pattern::parse("/x/[a/b]"), Error::UnterminatedClass));
    static_assert(faults(Pattern::parse("/{a,b}{c"), Error::UnterminatedBraces));
    static_assert(faults(Pattern::parse("/[a{"), Error::UnterminatedClass));
}

TEST_CASE("a pattern keeps a view of the text it was parsed from")
{
    constexpr std::string_view text = "/synth/*/freq";
    constexpr Pattern::ParseResult parsed = Pattern::parse(text);
    static_assert(parsed.pattern().text() == text);
    static_assert(parsed.pattern().text().data() == text.data());
}

TEST_CASE("matches agrees with match")
{
    static_assert(Pattern::parse("/synth/*/freq").pattern().matches("/synth/1/freq"));
    static_assert(!Pattern::parse("/synth/*/freq").pattern().matches("/synth/1/amp"));
    static_assert(Pattern::parse("/a//").pattern().matches("/a/b/c"));
    static_assert(Pattern::parse("/a*b*c/[!x]?/{ab,a}b").pattern().matches("/aXbYc/y1/abb"));
    static_assert(!Pattern::parse("/a").pattern().matches("a"));
    static_assert(!Pattern::parse("/a").pattern().matches(""));
}

TEST_CASE("match is parse followed by matches")
{
    static_assert(oscpm::match("/synth/*/freq", "/synth/1/freq"));
    static_assert(!oscpm::match("/synth/[1-3", "/synth/1"));
    static_assert(!oscpm::match("synth", "synth"));
}

TEST_CASE("a pattern copied out of its parse result still matches")
{
    const std::string owned = "/mixer/ch[1-4]/{amp,freq}";
    Pattern copied = Pattern::parse(owned).pattern();
    CHECK(copied.matches("/mixer/ch2/freq"));
    CHECK_FALSE(copied.matches("/mixer/ch5/freq"));
    CHECK(copied.text() == owned);

    const Pattern assigned = copied;
    CHECK(assigned.matches("/mixer/ch4/amp"));
    CHECK(assigned.text().data() == owned.data());
}

TEST_CASE("a pattern is literal when it contains no wildcard, class, brace list or slash run")
{
    static_assert(Pattern::parse("/synth/1/freq").pattern().isLiteral());
    static_assert(Pattern::parse("/").pattern().isLiteral());
    static_assert(Pattern::parse("/a/").pattern().isLiteral());
    static_assert(Pattern::parse("/a]").pattern().isLiteral());
    static_assert(Pattern::parse("/a}").pattern().isLiteral());
    static_assert(Pattern::parse("/a,b").pattern().isLiteral());
    static_assert(Pattern::parse("/#bundle").pattern().isLiteral());
    static_assert(Pattern::parse("/a-b").pattern().isLiteral());
    static_assert(!Pattern::parse("/synth/*").pattern().isLiteral());
    static_assert(!Pattern::parse("/synth/?").pattern().isLiteral());
    static_assert(!Pattern::parse("/synth/[1]").pattern().isLiteral());
    static_assert(!Pattern::parse("/synth/{a}").pattern().isLiteral());
    static_assert(!Pattern::parse("//gain").pattern().isLiteral());
    static_assert(!Pattern::parse("/a//b").pattern().isLiteral());
    static_assert(!Pattern::parse("/a//").pattern().isLiteral());
    static_assert(!Pattern::parse("//").pattern().isLiteral());
}

TEST_CASE("a pattern with a part beyond the maximum length is not literal and never matches")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(Pattern::parse("/" + longest).pattern().isLiteral());
    CHECK(Pattern::parse("/" + longest).pattern().matches("/" + longest));
    CHECK_FALSE(Pattern::parse("/" + tooLong).pattern().isLiteral());
    CHECK_FALSE(Pattern::parse("/" + tooLong).pattern().matches("/" + tooLong));
    CHECK_FALSE(Pattern::parse("/a/" + tooLong).pattern().isLiteral());
}

TEST_CASE("every byte the matcher treats as an opener makes a pattern non-literal and no other does")
{
    for (int value = 0; value < 256; ++value)
    {
        const char byte = static_cast<char>(value);
        if (byte == '/')
        {
            continue;
        }
        INFO("byte " << value);
        const char other = byte == 'z' ? 'y' : 'z';
        const std::string self = std::string("/") + byte + "x";
        const Pattern::ParseResult parsed = Pattern::parse(self);
        const bool literalByFlag = parsed && parsed.pattern().isLiteral();
        const bool literalByBehaviour = parsed
            && oscpm::match(self, self)
            && !oscpm::match(self, "/x")
            && !oscpm::match(self, std::string("/") + other + "x")
            && !oscpm::match(self, std::string("/") + byte + other + "x");
        CHECK(literalByFlag == literalByBehaviour);
    }
}

TEST_CASE("the literal shortcut agrees with the general matcher")
{
    const char* const patterns[] = { "/", "/a", "/a/", "/a/b", "/a]", "/a}", "/a,b", "/#bundle", "/a b", "/synth/1/freq" };
    const char* const addresses[] = { "", "a", "/", "/a", "/a/", "/a/b", "/a]", "/a}", "/a,b", "/#bundle", "/a b", "/synth/1/freq", "/synth/1/fre", "/synth/1/freq/" };
    for (const char* pattern : patterns)
    {
        const Pattern::ParseResult parsed = Pattern::parse(pattern);
        REQUIRE(parsed);
        REQUIRE(parsed.pattern().isLiteral());
        for (const char* address : addresses)
        {
            INFO("pattern " << pattern << " address " << address);
            CHECK(parsed.pattern().matches(address) == oscpm::detail::matchParsed(pattern, address));
        }
    }
}

TEST_CASE("a literal pattern matches only its own text")
{
    static_assert(Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/freq"));
    static_assert(!Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/fre"));
    static_assert(!Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/freq/"));
    static_assert(Pattern::parse("/a]").pattern().matches("/a]"));
    static_assert(!Pattern::parse("/a]").pattern().matches("/a"));
}

TEST_CASE("parsing and matching allocate nothing")
{
    const std::string text = "/a*b*c/[!x]?/{ab,a}b";
    const std::string address = "/aXbYc/y1/abb";
    const std::string malformed = "/synth/[1-3";

    const std::size_t before = oscpm_test::allocationCount();
    const Pattern::ParseResult parsed = Pattern::parse(text);
    const bool matched = parsed.pattern().matches(address);
    const bool convenience = oscpm::match(text, address);
    const Pattern::ParseResult failed = Pattern::parse(malformed);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed);
    CHECK(matched);
    CHECK(convenience);
    CHECK_FALSE(failed);
}

TEST_CASE("the allocation counter observes the heap")
{
    const std::size_t before = oscpm_test::allocationCount();
    void* const allocated = ::operator new(sizeof(int));
    CHECK(oscpm_test::allocationCount() > before);
    ::operator delete(allocated);
}
