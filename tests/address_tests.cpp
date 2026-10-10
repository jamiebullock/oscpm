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
#include <string>
#include <string_view>
#include <type_traits>

using oscpm::Address;
using oscpm::AddressError;

namespace
{

constexpr oscpm::Expected<Address, oscpm::AddressError> kFrequency = Address::parse("/synth/1/freq");
constexpr oscpm::Expected<Address, oscpm::AddressError> kTrailing = Address::parse("/synth/1/");

static_assert(kFrequency);
static_assert(kFrequency->text() == "/synth/1/freq");
static_assert(!kTrailing);
static_assert(kTrailing.error() == AddressError::TrailingSlash);
static_assert(std::is_trivially_copyable_v<Address>);
static_assert(std::is_trivially_copyable_v<oscpm::Expected<Address, oscpm::AddressError>>);

constexpr bool faults(const oscpm::Expected<Address, oscpm::AddressError>& result, AddressError kind)
{
    return !result && result.error() == kind;
}

}

TEST_CASE("parse yields an address for a well-formed address")
{
    static_assert(Address::parse("/a"));
    static_assert(Address::parse("/a/b/c"));
    static_assert(Address::parse("/!\"$%&'()+-.0123456789:;<=>@ABCXYZ\\^_`abcxyz|~"));
}

TEST_CASE("parse reports the first fault")
{
    static_assert(faults(Address::parse(""), AddressError::MissingLeadingSlash));
    static_assert(faults(Address::parse("synth"), AddressError::MissingLeadingSlash));
    static_assert(faults(Address::parse(" /a"), AddressError::MissingLeadingSlash));
    static_assert(faults(Address::parse("/"), AddressError::TrailingSlash));
    static_assert(faults(Address::parse("/a/b/"), AddressError::TrailingSlash));
    static_assert(faults(Address::parse("/synth//freq"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/a///b"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/a//?"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/synth/*"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/x/y/z*"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a\x7f"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a b"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a#"), AddressError::IllegalByte));
}

TEST_CASE("parse rejects a part longer than the supported length")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(Address::parse("/" + longest + "/" + longest));
    CHECK(faults(Address::parse("/" + tooLong), AddressError::PartTooLong));
    CHECK(faults(Address::parse("/a/" + tooLong), AddressError::PartTooLong));
}

TEST_CASE("an address keeps a view of the text it was parsed from")
{
    constexpr std::string_view text = "/synth/1/freq";
    constexpr oscpm::Expected<Address, oscpm::AddressError> parsed = Address::parse(text);
    static_assert(parsed->text() == text);
    static_assert(parsed->text().data() == text.data());
}

TEST_CASE("an address copied out of its parse result keeps its text")
{
    constexpr Address address = *Address::parse("/mixer/master/gain");
    static_assert(address.text() == "/mixer/master/gain");
}

TEST_CASE("an address parses as a literal pattern that matches itself")
{
    for (const char* text : { "/a", "/synth/1/freq", "/a.b-c_d", "/!\"$%&'()+-.:;<=>@\\^_`|~" })
    {
        INFO("address " << text);
        const oscpm::Expected<Address, oscpm::AddressError> address = Address::parse(text);
        const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> pattern = oscpm::Pattern::parse(text);
        REQUIRE(address);
        REQUIRE(pattern);
        CHECK(pattern->isLiteral());
        CHECK(pattern->matches(*address));
    }
}

TEST_CASE("parsing an address allocates nothing")
{
    const std::string text = "/synth/1/freq";
    const std::string malformed = "/synth/1/";

    const std::size_t before = oscpm_test::allocationCount();
    const oscpm::Expected<Address, oscpm::AddressError> parsed = Address::parse(text);
    const oscpm::Expected<Address, oscpm::AddressError> failed = Address::parse(malformed);
    const std::size_t after = oscpm_test::allocationCount();

    CHECK(after == before);
    CHECK(parsed);
    CHECK_FALSE(failed);
}
