/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
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
    STATIC_CHECK(match("/synth/1/freq", "/synth/1/freq"));
    STATIC_CHECK_FALSE(match("/synth/1/freq", "/synth/1/fre"));
    STATIC_CHECK(match("/x?y", "/x1y"));
    STATIC_CHECK_FALSE(match("/x?y", "/x/y"));
    STATIC_CHECK(match("/a*b*c", "/axxbyyc"));
    STATIC_CHECK_FALSE(match("/a*", "/a/b"));
    STATIC_CHECK(match("/[a-c-e]", "/-"));
    STATIC_CHECK_FALSE(match("/[z-a]", "/m"));
    STATIC_CHECK(match("/[!a-z]", "/1"));
    STATIC_CHECK(match("/[!]", "/]"));
    STATIC_CHECK_FALSE(match("/[]", "/a"));
    STATIC_CHECK(match("/{ab,a}c", "/ac"));
    STATIC_CHECK(match("/x{a,}", "/x"));
    STATIC_CHECK(match("/{a,{b,c}}", "/{b}"));
    STATIC_CHECK(match("//gain", "/synth/1/osc/2/gain"));
    STATIC_CHECK(match("/a//b//c", "/a/x/b/y/c"));
    STATIC_CHECK_FALSE(match("/a//c", "/ab/c"));
    STATIC_CHECK(match("/a///", "/a/b/c"));
    STATIC_CHECK(match("//", "/"));
    STATIC_CHECK_FALSE(match("/a/", "/a"));
    STATIC_CHECK(match("/a]", "/a]"));
    STATIC_CHECK(match("/caf[\xc3]?", "/caf\xc3\xa9"));
    STATIC_CHECK_FALSE(match("/", ""));
    STATIC_CHECK_FALSE(match("/a", "a"));
}

TEST_CASE("every pattern fault is reported at compile time and matches nothing")
{
    STATIC_CHECK(passes(validatePattern("/a/?/[a-z]/{x,y}")));
    STATIC_CHECK(passes(validatePattern("/{a,{b,c}}]} #\xc3\xa9")));
    STATIC_CHECK(faults(validatePattern(""), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validatePattern("a/b"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validatePattern("/x/[a/b]"), Error::UnterminatedClass, 3));
    STATIC_CHECK(faults(validatePattern("/{a,b}{c"), Error::UnterminatedBraces, 6));
    STATIC_CHECK(faults(validatePattern("/[a{"), Error::UnterminatedClass, 1));
    STATIC_CHECK_FALSE(match("", ""));
    STATIC_CHECK_FALSE(match("/[a", "/[a"));
    STATIC_CHECK_FALSE(match("/{a/b}", "/a/b"));
}

TEST_CASE("every address fault is reported at compile time")
{
    STATIC_CHECK(passes(validateAddress("/!\"$%&'()+-.0123456789:;<=>@ABCXYZ\\^_`abcxyz|~")));
    STATIC_CHECK(faults(validateAddress(" /a"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validateAddress("/"), Error::TrailingSlash, 0));
    STATIC_CHECK(faults(validateAddress("/a/b/"), Error::TrailingSlash, 4));
    STATIC_CHECK(faults(validateAddress("/a///b"), Error::EmptyPart, 3));
    STATIC_CHECK(faults(validateAddress("/x/y/z*"), Error::IllegalByte, 6));
    STATIC_CHECK(faults(validateAddress("/a\x7f"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a//?"), Error::EmptyPart, 3));
}

TEST_CASE("toString names every error")
{
    STATIC_CHECK(std::string_view(oscpm::toString(Error::MissingLeadingSlash)) == "MissingLeadingSlash");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::UnterminatedClass)) == "UnterminatedClass");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::UnterminatedBraces)) == "UnterminatedBraces");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::TrailingSlash)) == "TrailingSlash");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::EmptyPart)) == "EmptyPart");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::IllegalByte)) == "IllegalByte");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::PartTooLong)) == "PartTooLong");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::Duplicate)) == "Duplicate");
    STATIC_CHECK(std::string_view(oscpm::toString(Error::NotFound)) == "NotFound");
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
