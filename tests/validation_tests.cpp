/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>

using oscpm::Error;
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

}

TEST_CASE("a well-formed address validates")
{
    STATIC_CHECK(passes(validateAddress("/a")));
    STATIC_CHECK(passes(validateAddress("/synth/1/freq")));
    STATIC_CHECK(passes(validateAddress("/a-b_c.d~e!")));
    STATIC_CHECK(passes(validateAddress("/!\"$%&'()+-.0123456789:;<=>@ABCXYZ\\^_`abcxyz|~")));
}

TEST_CASE("an address starts with a slash")
{
    STATIC_CHECK(faults(validateAddress(""), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validateAddress("a"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validateAddress("a/b"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validateAddress(" /a"), Error::MissingLeadingSlash, 0));
}

TEST_CASE("a trailing slash is reported at the final slash")
{
    STATIC_CHECK(faults(validateAddress("/"), Error::TrailingSlash, 0));
    STATIC_CHECK(faults(validateAddress("/a/"), Error::TrailingSlash, 2));
    STATIC_CHECK(faults(validateAddress("/a/b/"), Error::TrailingSlash, 4));
}

TEST_CASE("an empty part is reported at the second of two adjacent slashes")
{
    STATIC_CHECK(faults(validateAddress("//"), Error::EmptyPart, 1));
    STATIC_CHECK(faults(validateAddress("//a"), Error::EmptyPart, 1));
    STATIC_CHECK(faults(validateAddress("/a//b"), Error::EmptyPart, 3));
    STATIC_CHECK(faults(validateAddress("/a///b"), Error::EmptyPart, 3));
    STATIC_CHECK(faults(validateAddress("/a//"), Error::EmptyPart, 3));
}

TEST_CASE("a pattern character, hash or space in an address is an illegal byte")
{
    STATIC_CHECK(faults(validateAddress("/a "), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a#"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a*"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a,"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a?"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a["), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a]"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a{"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a}"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/ /a"), Error::IllegalByte, 1));
    STATIC_CHECK(faults(validateAddress("/x/y/z*"), Error::IllegalByte, 6));
}

TEST_CASE("a byte outside printable ASCII in an address is an illegal byte")
{
    STATIC_CHECK(faults(validateAddress("/a\tb"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a\n"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a\x7f"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/caf\xc3\xa9"), Error::IllegalByte, 4));
}

TEST_CASE("the first address fault by byte offset wins")
{
    STATIC_CHECK(faults(validateAddress("a?"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validateAddress("/a?/"), Error::IllegalByte, 2));
    STATIC_CHECK(faults(validateAddress("/a/?/"), Error::IllegalByte, 3));
    STATIC_CHECK(faults(validateAddress("/a//?"), Error::EmptyPart, 3));
}

TEST_CASE("an address part beyond the maximum length faults at its first excess byte")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(passes(validateAddress("/" + longest)));
    CHECK(passes(validateAddress("/" + longest + "/" + longest)));
    CHECK(faults(validateAddress("/" + tooLong), Error::PartTooLong, oscpm::kMaxAddressPartLength + 1));
    CHECK(faults(validateAddress("/a/" + tooLong), Error::PartTooLong, oscpm::kMaxAddressPartLength + 3));
}

TEST_CASE("a pattern starts with a slash")
{
    STATIC_CHECK(passes(validatePattern("/a")));
    STATIC_CHECK(passes(validatePattern("/*")));
    STATIC_CHECK(passes(validatePattern("/a/?/[a-z]/{x,y}")));
    STATIC_CHECK(faults(validatePattern(""), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validatePattern("a"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validatePattern("a/b"), Error::MissingLeadingSlash, 0));
    STATIC_CHECK(faults(validatePattern(" /a"), Error::MissingLeadingSlash, 0));
}

TEST_CASE("an unterminated class is reported at its opening bracket")
{
    STATIC_CHECK(faults(validatePattern("/["), Error::UnterminatedClass, 1));
    STATIC_CHECK(faults(validatePattern("/[!"), Error::UnterminatedClass, 1));
    STATIC_CHECK(faults(validatePattern("/[a-"), Error::UnterminatedClass, 1));
    STATIC_CHECK(faults(validatePattern("/a[b"), Error::UnterminatedClass, 2));
    STATIC_CHECK(faults(validatePattern("/[a/b]"), Error::UnterminatedClass, 1));
    STATIC_CHECK(faults(validatePattern("/x/[a/b]"), Error::UnterminatedClass, 3));
    STATIC_CHECK(faults(validatePattern("/a//["), Error::UnterminatedClass, 4));
    STATIC_CHECK(faults(validatePattern("/{a,b}/[c"), Error::UnterminatedClass, 7));
}

TEST_CASE("an unterminated brace list is reported at its opening brace")
{
    STATIC_CHECK(faults(validatePattern("/{"), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/{a,"), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/x{a,b"), Error::UnterminatedBraces, 2));
    STATIC_CHECK(faults(validatePattern("/{a/b}"), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/{a{b"), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/{a[b"), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/{a,b}{c"), Error::UnterminatedBraces, 6));
    STATIC_CHECK(faults(validatePattern("/{a}/{b"), Error::UnterminatedBraces, 5));
}

TEST_CASE("the first pattern fault by byte offset wins")
{
    STATIC_CHECK(faults(validatePattern("/[a{"), Error::UnterminatedClass, 1));
    STATIC_CHECK(faults(validatePattern("/{a["), Error::UnterminatedBraces, 1));
    STATIC_CHECK(faults(validatePattern("/[a]/{b"), Error::UnterminatedBraces, 5));
}

TEST_CASE("a class or brace list closes at the first closer within its part")
{
    STATIC_CHECK(passes(validatePattern("/[abc]")));
    STATIC_CHECK(passes(validatePattern("/[]")));
    STATIC_CHECK(passes(validatePattern("/[!]")));
    STATIC_CHECK(passes(validatePattern("/[]a]")));
    STATIC_CHECK(passes(validatePattern("/[[]")));
    STATIC_CHECK(passes(validatePattern("/[{]")));
    STATIC_CHECK(passes(validatePattern("/[*?,]")));
    STATIC_CHECK(passes(validatePattern("/{}")));
    STATIC_CHECK(passes(validatePattern("/{a,}")));
    STATIC_CHECK(passes(validatePattern("/{a*,[b]}")));
    STATIC_CHECK(passes(validatePattern("/{a,{b,c}}")));
    STATIC_CHECK(passes(validatePattern("/{a[b],c}")));
    STATIC_CHECK(passes(validatePattern("/[a]/[b]")));
    STATIC_CHECK(passes(validatePattern("/{a}/{b}")));
}

TEST_CASE("a stray closer, comma, hash, space or non-ASCII byte in a pattern parses")
{
    STATIC_CHECK(passes(validatePattern("/a]")));
    STATIC_CHECK(passes(validatePattern("/a}")));
    STATIC_CHECK(passes(validatePattern("/]")));
    STATIC_CHECK(passes(validatePattern("/,")));
    STATIC_CHECK(passes(validatePattern("/#bundle")));
    STATIC_CHECK(passes(validatePattern("/a b")));
    STATIC_CHECK(passes(validatePattern("/a\x7f")));
    STATIC_CHECK(passes(validatePattern("/caf\xc3\xa9")));
}

TEST_CASE("empty parts and slash runs in a pattern parse")
{
    STATIC_CHECK(passes(validatePattern("/")));
    STATIC_CHECK(passes(validatePattern("//")));
    STATIC_CHECK(passes(validatePattern("///")));
    STATIC_CHECK(passes(validatePattern("//a")));
    STATIC_CHECK(passes(validatePattern("/a//b")));
    STATIC_CHECK(passes(validatePattern("/a/")));
    STATIC_CHECK(passes(validatePattern("/a//")));
}

TEST_CASE("a malformed pattern matches nothing")
{
    STATIC_CHECK_FALSE(oscpm::match("", ""));
    STATIC_CHECK_FALSE(oscpm::match("a", "a"));
    STATIC_CHECK_FALSE(oscpm::match("a/b", "a/b"));
    STATIC_CHECK_FALSE(oscpm::match("/[a", "/[a"));
    STATIC_CHECK_FALSE(oscpm::match("/{a", "/{a"));
    STATIC_CHECK_FALSE(oscpm::match("/[a/b]", "/a/b"));
    STATIC_CHECK_FALSE(oscpm::match("/{a/b}", "/a/b"));
}

TEST_CASE("every well-formed address is a pattern that matches itself")
{
    STATIC_CHECK(passes(validateAddress("/synth/1/freq")));
    STATIC_CHECK(passes(validatePattern("/synth/1/freq")));
    STATIC_CHECK(oscpm::match("/synth/1/freq", "/synth/1/freq"));
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
