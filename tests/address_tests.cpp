/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <oscpm/address.h>
#include <oscpm/detail/validate.h>
#include <oscpm/pattern.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <ostream>
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
    static_assert(faults(Address::parse("/a,"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a?"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a["), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a]"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a{"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a}"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a\x80"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/a\xff"), AddressError::IllegalByte));
}

TEST_CASE("parse reports the first fault of an address longer than a word")
{
    static_assert(Address::parse("/synth3/voice/7/osc/saw/freq"));
    static_assert(faults(Address::parse("/abcdefg//jklmnop"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/abcdefgh//klmnop"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/abcdefgh/"), AddressError::TrailingSlash));
    static_assert(faults(Address::parse("/abcdefghijklmnop/"), AddressError::TrailingSlash));
    static_assert(faults(Address::parse("/abcdefghijklmnop\x80"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/ab//cde*"), AddressError::EmptyPart));
    static_assert(faults(Address::parse("/abc*d//"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/abcdefgh*//"), AddressError::IllegalByte));
    static_assert(faults(Address::parse("/abcdefg//jklmn*"), AddressError::EmptyPart));
}

TEST_CASE("parse judges a byte the same at every offset of a word")
{
    constexpr std::string_view legal = "/abcdefghijklmnopqrstuvw";
    constexpr std::string_view reserved = "#*,?[]{}";
    for (std::size_t offset = 1; offset < legal.size(); ++offset)
    {
        for (int value = 0; value < 256; ++value)
        {
            std::string text(legal);
            text[offset] = static_cast<char>(value);
            INFO("byte " << value << " at " << offset);
            const oscpm::Expected<Address, oscpm::AddressError> parsed = Address::parse(text);
            const std::optional<AddressError> byByte = oscpm::detail::validateAddressBytes(text, 1, 1);
            CHECK(parsed.has_value() == !byByte.has_value());
            CHECK((byByte && faults(parsed, *byByte)) == byByte.has_value());
            if (value == '/')
            {
                const bool first = offset == 1;
                const bool last = offset == legal.size() - 1;
                CHECK((!first && !last) == parsed.has_value());
                CHECK((first && faults(parsed, AddressError::EmptyPart)) == first);
                CHECK((last && faults(parsed, AddressError::TrailingSlash)) == last);
            }
            else
            {
                const bool printable = value >= 0x21 && value <= 0x7E;
                const bool inPart = printable && reserved.find(static_cast<char>(value)) == std::string_view::npos;
                CHECK(inPart == parsed.has_value());
                CHECK((!inPart && faults(parsed, AddressError::IllegalByte)) == !inPart);
            }
        }
    }
}

TEST_CASE("parse reads no byte beyond the text")
{
    constexpr std::string_view buffer = "/abcdefghijklmnopq//*";
    constexpr std::string_view text = buffer.substr(0, 18);
    static_assert(Address::parse(text));
    static_assert(faults(Address::parse(buffer.substr(0, 19)), AddressError::TrailingSlash));
}

TEST_CASE("parse rejects a part longer than the supported length")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    CHECK(Address::parse("/" + longest + "/" + longest));
    CHECK(faults(Address::parse("/" + tooLong), AddressError::PartTooLong));
    CHECK(faults(Address::parse("/a/" + tooLong), AddressError::PartTooLong));
}

TEST_CASE("parse rejects a part over the limit whatever precedes it")
{
    const std::string longest(oscpm::kMaxAddressPartLength, 'a');
    const std::string tooLong(oscpm::kMaxAddressPartLength + 1, 'a');
    for (std::size_t prefix = 0; prefix < 16; ++prefix)
    {
        const std::string head = prefix == 0 ? "/" : "/" + std::string(prefix, 'p') + "/";
        INFO("prefix " << head);
        CHECK(Address::parse(head + longest));
        CHECK(Address::parse(head + longest + "/" + longest));
        CHECK(faults(Address::parse(head + tooLong), AddressError::PartTooLong));
        CHECK(faults(Address::parse(head + longest + "//"), AddressError::EmptyPart));
        CHECK(faults(Address::parse(head + tooLong + "//"), AddressError::PartTooLong));
        CHECK(faults(Address::parse(head + longest + "/"), AddressError::TrailingSlash));
    }
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

TEST_CASE("the constructor yields the address parse yields")
{
    constexpr Address address("/synth/1/freq");
    static_assert(address.text() == kFrequency->text());

    const std::string text = "/mixer/" + std::to_string(3) + "/gain";
    const Address built(text);
    CHECK(built.text() == Address::parse(text)->text());
    CHECK(built.text().data() == text.data());
}
