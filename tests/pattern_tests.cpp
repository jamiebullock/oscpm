/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

using oscpm::Error;
using oscpm::ParseError;
using oscpm::ParseResult;
using oscpm::Pattern;

namespace
{

constexpr ParseResult kWildcard = Pattern::parse("/synth/*/{freq,amp}");
constexpr ParseResult kDescendant = Pattern::parse("//gain");
constexpr ParseResult kUnterminated = Pattern::parse("/synth/[1-3");

static_assert(kWildcard);
static_assert(kWildcard.pattern().matches("/synth/12/amp"));
static_assert(!kWildcard.pattern().matches("/synth/12/gain"));
static_assert(kDescendant.pattern().matches("/mixer/bus/3/gain"));
static_assert(!kUnterminated);
static_assert(kUnterminated.error().kind == Error::UnterminatedClass);
static_assert(kUnterminated.error().offset == 7);
static_assert(std::is_trivially_copyable_v<Pattern>);
static_assert(std::is_trivially_copyable_v<ParseResult>);

constexpr bool faults(const ParseResult& result, Error kind, std::size_t offset)
{
    return !result && result.error().kind == kind && result.error().offset == offset;
}

}

TEST_CASE("parse yields a pattern for a well-formed pattern")
{
    STATIC_CHECK(Pattern::parse("/a"));
    STATIC_CHECK(Pattern::parse("/"));
    STATIC_CHECK(Pattern::parse("//"));
    STATIC_CHECK(Pattern::parse("/a/?/[a-z]/{x,y}"));
    STATIC_CHECK(Pattern::parse("/{a,{b,c}}"));
    STATIC_CHECK(Pattern::parse("/a]"));
    STATIC_CHECK(Pattern::parse("/caf\xc3\xa9"));
}

TEST_CASE("parse reports the fault validatePattern reports")
{
    STATIC_CHECK(faults(Pattern::parse(""), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(Pattern::parse("a/b"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(Pattern::parse("/a[b"), Error::UnterminatedClass, 2));
    STATIC_CHECK(faults(Pattern::parse("/{a,b}{c"), Error::UnterminatedBraces, 6));
    STATIC_CHECK(faults(Pattern::parse("/[a{"), Error::UnterminatedClass, 1));
}

TEST_CASE("a pattern keeps a view of the text it was parsed from")
{
    constexpr std::string_view text = "/synth/*/freq";
    constexpr ParseResult parsed = Pattern::parse(text);
    STATIC_CHECK(parsed.pattern().text() == text);
    STATIC_CHECK(parsed.pattern().text().data() == text.data());
}

TEST_CASE("matches agrees with match")
{
    STATIC_CHECK(Pattern::parse("/synth/*/freq").pattern().matches("/synth/1/freq"));
    STATIC_CHECK_FALSE(Pattern::parse("/synth/*/freq").pattern().matches("/synth/1/amp"));
    STATIC_CHECK(Pattern::parse("/a//").pattern().matches("/a/b/c"));
    STATIC_CHECK(Pattern::parse("/a*b*c/[!x]?/{ab,a}b").pattern().matches("/aXbYc/y1/abb"));
    STATIC_CHECK_FALSE(Pattern::parse("/a").pattern().matches("a"));
    STATIC_CHECK_FALSE(Pattern::parse("/a").pattern().matches(""));
}

TEST_CASE("match is parse followed by matches")
{
    STATIC_CHECK(oscpm::match("/synth/*/freq", "/synth/1/freq"));
    STATIC_CHECK_FALSE(oscpm::match("/synth/[1-3", "/synth/1"));
    STATIC_CHECK_FALSE(oscpm::match("synth", "synth"));
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
    STATIC_CHECK(Pattern::parse("/synth/1/freq").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/a/").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/a]").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/a}").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/a,b").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/#bundle").pattern().isLiteral());
    STATIC_CHECK(Pattern::parse("/a-b").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/synth/*").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/synth/?").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/synth/[1]").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/synth/{a}").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("//gain").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/a//b").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("/a//").pattern().isLiteral());
    STATIC_CHECK_FALSE(Pattern::parse("//").pattern().isLiteral());
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
        const ParseResult parsed = Pattern::parse(self);
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
        const ParseResult parsed = Pattern::parse(pattern);
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
    STATIC_CHECK(Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/freq"));
    STATIC_CHECK_FALSE(Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/fre"));
    STATIC_CHECK_FALSE(Pattern::parse("/synth/1/freq").pattern().matches("/synth/1/freq/"));
    STATIC_CHECK(Pattern::parse("/a]").pattern().matches("/a]"));
    STATIC_CHECK_FALSE(Pattern::parse("/a]").pattern().matches("/a"));
}

TEST_CASE("parsing matching and validating allocate nothing")
{
    const std::string text = "/a*b*c/[!x]?/{ab,a}b";
    const std::string address = "/aXbYc/y1/abb";
    const std::string malformed = "/synth/[1-3";

    const std::size_t before = oscpm_test::allocationCount();
    const ParseResult parsed = Pattern::parse(text);
    const bool matched = parsed.pattern().matches(address);
    const bool convenience = oscpm::match(text, address);
    const ParseResult failed = Pattern::parse(malformed);
    const std::optional<ParseError> patternFault = oscpm::validatePattern(malformed);
    const std::optional<ParseError> addressFault = oscpm::validateAddress(address);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed);
    CHECK(matched);
    CHECK(convenience);
    CHECK_FALSE(failed);
    CHECK(patternFault.has_value());
    CHECK_FALSE(addressFault.has_value());
}

TEST_CASE("the allocation counter observes the heap")
{
    const std::size_t before = oscpm_test::allocationCount();
    void* const allocated = ::operator new(sizeof(int));
    CHECK(oscpm_test::allocationCount() > before);
    ::operator delete(allocated);
}
