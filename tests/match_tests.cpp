/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

using oscpm::Error;
using oscpm::match;
using oscpm::ParseError;
using oscpm::validateAddress;
using oscpm::validatePattern;

namespace
{

constexpr bool faults(const std::optional<ParseError>& result, Error kind, std::size_t offset)
{
    return result.has_value() && result->kind == kind && result->offset == offset;
}

constexpr bool passes(const std::optional<ParseError>& result)
{
    return !result.has_value();
}

template <typename Function>
double secondsTaken(Function&& function)
{
    const auto start = std::chrono::steady_clock::now();
    function();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

constexpr double kGenerousSeconds = 5.0;

}

TEST_CASE("every pattern construct is matched at compile time")
{
    static_assert(match("/synth/1/freq", "/synth/1/freq"));
    static_assert(!match("/synth/1/freq", "/synth/1/fre"));
    static_assert(match("/x?y", "/x1y"));
    static_assert(!match("/x?y", "/x/y"));
    static_assert(match("/a*b*c", "/axxbyyc"));
    static_assert(!match("/a*", "/a/b"));
    static_assert(match("/[a-c-e]", "/-"));
    static_assert(!match("/[z-a]", "/m"));
    static_assert(match("/[!a-z]", "/1"));
    static_assert(match("/[!]", "/]"));
    static_assert(!match("/[]", "/a"));
    static_assert(match("/{ab,a}c", "/ac"));
    static_assert(match("/x{a,}", "/x"));
    static_assert(match("/{a,{b,c}}", "/{b}"));
    static_assert(match("//gain", "/synth/1/osc/2/gain"));
    static_assert(match("/a//b//c", "/a/x/b/y/c"));
    static_assert(!match("/a//c", "/ab/c"));
    static_assert(match("/a///", "/a/b/c"));
    static_assert(match("//", "/"));
    static_assert(!match("/a/", "/a"));
    static_assert(match("/a]", "/a]"));
    static_assert(match("/caf[\xc3]?", "/caf\xc3\xa9"));
    static_assert(!match("/", ""));
    static_assert(!match("/a", "a"));
}

TEST_CASE("every pattern fault is reported at compile time and matches nothing")
{
    static_assert(passes(validatePattern("/a/?/[a-z]/{x,y}")));
    static_assert(passes(validatePattern("/{a,{b,c}}]} #\xc3\xa9")));
    static_assert(faults(validatePattern(""), Error::MissingLeadingSlash, 0));
    static_assert(faults(validatePattern("a/b"), Error::MissingLeadingSlash, 0));
    static_assert(faults(validatePattern("/x/[a/b]"), Error::UnterminatedClass, 3));
    static_assert(faults(validatePattern("/{a,b}{c"), Error::UnterminatedBraces, 6));
    static_assert(faults(validatePattern("/[a{"), Error::UnterminatedClass, 1));
    static_assert(!match("", ""));
    static_assert(!match("/[a", "/[a"));
    static_assert(!match("/{a/b}", "/a/b"));
}

TEST_CASE("every address fault is reported at compile time")
{
    static_assert(passes(validateAddress("/!\"$%&'()+-.0123456789:;<=>@ABCXYZ\\^_`abcxyz|~")));
    static_assert(faults(validateAddress(" /a"), Error::MissingLeadingSlash, 0));
    static_assert(faults(validateAddress("/"), Error::TrailingSlash, 0));
    static_assert(faults(validateAddress("/a/b/"), Error::TrailingSlash, 4));
    static_assert(faults(validateAddress("/a///b"), Error::EmptyPart, 3));
    static_assert(faults(validateAddress("/x/y/z*"), Error::IllegalByte, 6));
    static_assert(faults(validateAddress("/a\x7f"), Error::IllegalByte, 2));
    static_assert(faults(validateAddress("/a//?"), Error::EmptyPart, 3));
}

TEST_CASE("toString names every error")
{
    static_assert(std::string_view(oscpm::toString(Error::MissingLeadingSlash)) == "MissingLeadingSlash");
    static_assert(std::string_view(oscpm::toString(Error::UnterminatedClass)) == "UnterminatedClass");
    static_assert(std::string_view(oscpm::toString(Error::UnterminatedBraces)) == "UnterminatedBraces");
    static_assert(std::string_view(oscpm::toString(Error::PatternTooLong)) == "PatternTooLong");
    static_assert(std::string_view(oscpm::toString(Error::TrailingSlash)) == "TrailingSlash");
    static_assert(std::string_view(oscpm::toString(Error::EmptyPart)) == "EmptyPart");
    static_assert(std::string_view(oscpm::toString(Error::IllegalByte)) == "IllegalByte");
    static_assert(std::string_view(oscpm::toString(Error::PartTooLong)) == "PartTooLong");
    static_assert(std::string_view(oscpm::toString(Error::Duplicate)) == "Duplicate");
    static_assert(std::string_view(oscpm::toString(Error::NotFound)) == "NotFound");
}

TEST_CASE("an address part longer than the supported length never matches and is reported")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(match("/" + longest, "/" + longest));
    CHECK(match("/*", "/" + longest));
    CHECK_FALSE(match("/" + tooLong, "/" + tooLong));
    CHECK_FALSE(match("/*", "/" + tooLong));
    CHECK(passes(validateAddress("/" + longest + "/" + longest)));
    CHECK(faults(validateAddress("/" + tooLong), Error::PartTooLong, oscpm::kMaxAddressPartLength + 1));
    CHECK(faults(validateAddress("/a/" + tooLong), Error::PartTooLong, oscpm::kMaxAddressPartLength + 3));
}

TEST_CASE("a wildcard pattern longer than the supported length is reported and matches nothing")
{
    const std::string longest = "/*" + std::string(oscpm::kMaxPatternLength - 2, 'a');
    const std::string tooLong = "/*" + std::string(oscpm::kMaxPatternLength - 1, 'a');
    const std::string address = "/" + std::string(oscpm::kMaxPatternLength, 'a');
    CHECK(passes(validatePattern(longest)));
    CHECK(match(longest, address));
    CHECK(faults(validatePattern(tooLong), Error::PatternTooLong, oscpm::kMaxPatternLength));
    CHECK_FALSE(match(tooLong, address));
    const std::string many = "/" + std::string(oscpm::kMaxPatternLength, 'a');
    for (const std::string& wildcard : { many + "?", many + "*", many + "[a]", many + "{a}", many + "//a", "//" + many })
    {
        INFO("pattern " << wildcard.substr(wildcard.size() - 4));
        CHECK(faults(validatePattern(wildcard), Error::PatternTooLong, oscpm::kMaxPatternLength));
    }
}

TEST_CASE("a literal pattern longer than the wildcard limit parses and matches")
{
    const std::string literal = "/" + std::string(2000, 'a') + "/" + std::string(2000, 'b');
    CHECK(passes(validatePattern(literal)));
    CHECK(match(literal, literal));
    CHECK_FALSE(match(literal, literal + "c"));
}

TEST_CASE("a fault before the length limit is reported ahead of the length")
{
    CHECK(faults(validatePattern("/[" + std::string(2000, 'a')), Error::UnterminatedClass, 1));
    CHECK(faults(validatePattern("/" + std::string(1000, 'a') + "{" + std::string(2000, 'a')), Error::UnterminatedBraces, 1001));
    CHECK(faults(validatePattern("/" + std::string(1000, 'a') + "[" + std::string(100, 'b') + "]"), Error::PatternTooLong, oscpm::kMaxPatternLength));
    CHECK(faults(validatePattern("/" + std::string(1100, 'a') + "["), Error::PatternTooLong, oscpm::kMaxPatternLength));
}

TEST_CASE("every construct matches a part either side of 64 bytes")
{
    const std::size_t lengths[] = { 62, 63, 64, 65, 127, 128 };
    for (const std::size_t length : lengths)
    {
        INFO("part length " << length);
        const std::string part(length, 'a');
        const std::string lastByteDiffers = std::string(length - 1, 'a') + "b";
        const std::string allButLast(length - 1, 'a');
        for (const std::string& pattern : { "/" + allButLast + "?", std::string("/*a"), "/" + allButLast + "[a-c]", "/" + allButLast + "{x,a}", std::string("/*a*") })
        {
            INFO("pattern " << pattern);
            CHECK(match(pattern, "/" + part));
        }
        CHECK_FALSE(match("/*a", "/" + lastByteDiffers));
        CHECK_FALSE(match("/" + allButLast + "[!b]", "/" + lastByteDiffers));
        CHECK_FALSE(match("/" + allButLast + "{x,a}", "/" + lastByteDiffers));
        CHECK_FALSE(match("/" + allButLast + "??", "/" + part));
    }
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
