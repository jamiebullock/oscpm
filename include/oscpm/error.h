/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace oscpm
{

/// The maximum length, in bytes, of one part of an address.
constexpr std::size_t kMaxAddressPartLength = 4095;

/// The maximum length, in bytes, of a pattern containing a wildcard, class,
/// brace list or "//". A literal pattern has no limit.
constexpr std::size_t kMaxPatternLength = 1024;

/// A fault that stops a pattern parsing.
enum class PatternError : std::uint8_t
{
    MissingLeadingSlash, ///< no leading '/'
    UnterminatedClass, ///< a '[' with no ']' before the next '/'
    UnterminatedBraces, ///< a '{' with no '}' before the next '/'
    PatternTooLong ///< a wildcard pattern longer than `kMaxPatternLength`
};

/// A fault that stops an address parsing.
enum class AddressError : std::uint8_t
{
    MissingLeadingSlash, ///< no leading '/'
    TrailingSlash, ///< a final '/', including the bare "/"
    EmptyPart, ///< two adjacent slashes
    IllegalByte, ///< a byte outside printable ASCII or one of " #*,?[]{}"
    PartTooLong ///< a part longer than `kMaxAddressPartLength`
};

/// The enumerator's name as a null-terminated literal with static storage,
/// for diagnostics.
constexpr const char* toString(PatternError error) noexcept
{
    switch (error)
    {
    case PatternError::MissingLeadingSlash:
        return "MissingLeadingSlash";
    case PatternError::UnterminatedClass:
        return "UnterminatedClass";
    case PatternError::UnterminatedBraces:
        return "UnterminatedBraces";
    case PatternError::PatternTooLong:
        return "PatternTooLong";
    }
    return "";
}

/// The enumerator's name as a null-terminated literal with static storage,
/// for diagnostics.
constexpr const char* toString(AddressError error) noexcept
{
    switch (error)
    {
    case AddressError::MissingLeadingSlash:
        return "MissingLeadingSlash";
    case AddressError::TrailingSlash:
        return "TrailingSlash";
    case AddressError::EmptyPart:
        return "EmptyPart";
    case AddressError::IllegalByte:
        return "IllegalByte";
    case AddressError::PartTooLong:
        return "PartTooLong";
    }
    return "";
}

}
