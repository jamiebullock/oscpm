/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/error.h>

#include <doctest/doctest.h>

#include <string_view>

using oscpm::Error;

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
