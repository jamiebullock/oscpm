/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/error.h>

#include <doctest/doctest.h>

#include <string_view>

using oscpm::AddressError;
using oscpm::PatternError;

TEST_CASE("toString names every pattern error")
{
    static_assert(std::string_view(oscpm::toString(PatternError::MissingLeadingSlash)) == "MissingLeadingSlash");
    static_assert(std::string_view(oscpm::toString(PatternError::UnterminatedClass)) == "UnterminatedClass");
    static_assert(std::string_view(oscpm::toString(PatternError::UnterminatedBraces)) == "UnterminatedBraces");
    static_assert(std::string_view(oscpm::toString(PatternError::PatternTooLong)) == "PatternTooLong");
}

TEST_CASE("toString names every address error")
{
    static_assert(std::string_view(oscpm::toString(AddressError::MissingLeadingSlash)) == "MissingLeadingSlash");
    static_assert(std::string_view(oscpm::toString(AddressError::TrailingSlash)) == "TrailingSlash");
    static_assert(std::string_view(oscpm::toString(AddressError::EmptyPart)) == "EmptyPart");
    static_assert(std::string_view(oscpm::toString(AddressError::IllegalByte)) == "IllegalByte");
    static_assert(std::string_view(oscpm::toString(AddressError::PartTooLong)) == "PartTooLong");
}
