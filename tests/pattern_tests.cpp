/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <oscpm/address.h>
#include <oscpm/pattern.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <new>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

using oscpm::Address;
using oscpm::Pattern;
using oscpm::PatternError;

namespace
{

constexpr oscpm::Expected<Pattern, oscpm::PatternError> kWildcard = Pattern::parse("/synth/*/{freq,amp}");
constexpr oscpm::Expected<Pattern, oscpm::PatternError> kDescendant = Pattern::parse("//gain");
constexpr oscpm::Expected<Pattern, oscpm::PatternError> kUnterminated = Pattern::parse("/synth/[1-3");

static_assert(kWildcard);
static_assert(kWildcard->matches(*Address::parse("/synth/12/amp")));
static_assert(!kWildcard->matches(*Address::parse("/synth/12/gain")));
static_assert(kDescendant->matches(*Address::parse("/mixer/bus/3/gain")));
static_assert(!kUnterminated);
static_assert(kUnterminated.error() == PatternError::UnterminatedClass);
static_assert(std::is_trivially_copyable_v<Pattern>);
static_assert(std::is_trivially_copyable_v<oscpm::Expected<Pattern, oscpm::PatternError>>);

constexpr bool faults(const oscpm::Expected<Pattern, oscpm::PatternError>& result, PatternError kind)
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
    static_assert(faults(Pattern::parse(""), PatternError::MissingLeadingSlash));
    static_assert(faults(Pattern::parse("a/b"), PatternError::MissingLeadingSlash));
    static_assert(faults(Pattern::parse("/a[b"), PatternError::UnterminatedClass));
    static_assert(faults(Pattern::parse("/x/[a/b]"), PatternError::UnterminatedClass));
    static_assert(faults(Pattern::parse("/{a,b}{c"), PatternError::UnterminatedBraces));
    static_assert(faults(Pattern::parse("/[a{"), PatternError::UnterminatedClass));
}

TEST_CASE("a pattern keeps a view of the text it was parsed from")
{
    constexpr std::string_view text = "/synth/*/freq";
    constexpr oscpm::Expected<Pattern, oscpm::PatternError> parsed = Pattern::parse(text);
    static_assert(parsed->text() == text);
    static_assert(parsed->text().data() == text.data());
}

TEST_CASE("matches agrees with match")
{
    static_assert(Pattern::parse("/synth/*/freq")->matches(*Address::parse("/synth/1/freq")));
    static_assert(!Pattern::parse("/synth/*/freq")->matches(*Address::parse("/synth/1/amp")));
    static_assert(Pattern::parse("/a//")->matches(*Address::parse("/a/b/c")));
    static_assert(Pattern::parse("/a*b*c/[!x]?/{ab,a}b")->matches(*Address::parse("/aXbYc/y1/abb")));
}

TEST_CASE("match parses both and is false when either is malformed")
{
    static_assert(oscpm::match("/synth/*/freq", "/synth/1/freq"));
    static_assert(!oscpm::match("/synth/[1-3", "/synth/1"));
    static_assert(!oscpm::match("synth", "synth"));
    static_assert(!oscpm::match("/a", "a"));
    static_assert(!oscpm::match("/a", ""));
    static_assert(!oscpm::match("/a/", "/a/"));
    static_assert(!oscpm::match("/", "/"));
    static_assert(!oscpm::match("/a#b", "/a#b"));
}

TEST_CASE("a pattern copied out of its parse result still matches")
{
    const std::string owned = "/mixer/ch[1-4]/{amp,freq}";
    Pattern copied = *Pattern::parse(owned);
    CHECK(copied.matches(*Address::parse("/mixer/ch2/freq")));
    CHECK_FALSE(copied.matches(*Address::parse("/mixer/ch5/freq")));
    CHECK(copied.text() == owned);

    const Pattern assigned = copied;
    CHECK(assigned.matches(*Address::parse("/mixer/ch4/amp")));
    CHECK(assigned.text().data() == owned.data());
}

TEST_CASE("a pattern is literal when it contains no wildcard, class, brace list or slash run")
{
    static_assert(Pattern::parse("/synth/1/freq")->isLiteral());
    static_assert(Pattern::parse("/")->isLiteral());
    static_assert(Pattern::parse("/a/")->isLiteral());
    static_assert(Pattern::parse("/a]")->isLiteral());
    static_assert(Pattern::parse("/a}")->isLiteral());
    static_assert(Pattern::parse("/a,b")->isLiteral());
    static_assert(Pattern::parse("/#bundle")->isLiteral());
    static_assert(Pattern::parse("/a-b")->isLiteral());
    static_assert(!Pattern::parse("/synth/*")->isLiteral());
    static_assert(!Pattern::parse("/synth/?")->isLiteral());
    static_assert(!Pattern::parse("/synth/[1]")->isLiteral());
    static_assert(!Pattern::parse("/synth/{a}")->isLiteral());
    static_assert(!Pattern::parse("//gain")->isLiteral());
    static_assert(!Pattern::parse("/a//b")->isLiteral());
    static_assert(!Pattern::parse("/a//")->isLiteral());
    static_assert(!Pattern::parse("//")->isLiteral());
}

TEST_CASE("a pattern with a part beyond the maximum length is not literal and never matches")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(Pattern::parse("/" + longest)->isLiteral());
    CHECK(Pattern::parse("/" + longest)->matches(*Address::parse("/" + longest)));
    CHECK_FALSE(Pattern::parse("/" + tooLong)->isLiteral());
    CHECK_FALSE(oscpm::detail::matchParsed("/" + tooLong, "/" + tooLong));
    CHECK_FALSE(Pattern::parse("/a/" + tooLong)->isLiteral());
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
        const oscpm::Expected<Pattern, oscpm::PatternError> parsed = Pattern::parse(self);
        const bool literalByFlag = parsed && parsed->isLiteral();
        const bool literalByBehaviour = parsed
            && oscpm::detail::matchParsed(self, self)
            && !oscpm::detail::matchParsed(self, "/x")
            && !oscpm::detail::matchParsed(self, std::string("/") + other + "x")
            && !oscpm::detail::matchParsed(self, std::string("/") + byte + other + "x");
        CHECK(literalByFlag == literalByBehaviour);
    }
}

TEST_CASE("the literal shortcut agrees with the general matcher")
{
    const char* const patterns[] = { "/", "/a", "/a/", "/a/b", "/a]", "/a}", "/a,b", "/#bundle", "/a b", "/synth/1/freq" };
    const char* const addresses[] = { "/a", "/a/b", "/ab", "/synth/1/freq", "/synth/1/fre", "/synth/1/freq/x" };
    for (const char* pattern : patterns)
    {
        const oscpm::Expected<Pattern, oscpm::PatternError> parsed = Pattern::parse(pattern);
        REQUIRE(parsed);
        REQUIRE(parsed->isLiteral());
        for (const char* address : addresses)
        {
            INFO("pattern " << pattern << " address " << address);
            CHECK(parsed->matches(*Address::parse(address)) == oscpm::detail::matchParsed(pattern, address));
        }
    }
}

TEST_CASE("a literal pattern matches only its own text")
{
    static_assert(Pattern::parse("/synth/1/freq")->matches(*Address::parse("/synth/1/freq")));
    static_assert(!Pattern::parse("/synth/1/freq")->matches(*Address::parse("/synth/1/fre")));
    static_assert(!Pattern::parse("/synth/1/freq")->matches(*Address::parse("/synth/1/freq/x")));
    static_assert(!Pattern::parse("/a]")->matches(*Address::parse("/a")));
}

TEST_CASE("parsing and matching allocate nothing")
{
    const std::string text = "/a*b*c/[!x]?/{ab,a}b";
    const std::string address = "/aXbYc/y1/abb";
    const std::string malformed = "/synth/[1-3";

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::Expected<Pattern, oscpm::PatternError> parsed = Pattern::parse(text);
    const bool matched = parsed->matches(*Address::parse(address));
    const bool convenience = oscpm::match(text, address);
    const oscpm::Expected<Pattern, oscpm::PatternError> failed = Pattern::parse(malformed);
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
