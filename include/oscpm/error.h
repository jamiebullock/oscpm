/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>

namespace oscpm
{

/// The maximum length, in bytes, of one part of an address.
constexpr std::size_t kMaxAddressPartLength = 4095;

/// The maximum length, in bytes, of a pattern containing a wildcard, class,
/// brace list or "//". A literal pattern has no limit.
constexpr std::size_t kMaxPatternLength = 1024;

/// A fault in a pattern, an address or an `AddressSpace` operation.
enum class Error
{
    MissingLeadingSlash, ///< pattern or address: no leading '/'
    UnterminatedClass, ///< pattern: a '[' with no ']' before the next '/'
    UnterminatedBraces, ///< pattern: a '{' with no '}' before the next '/'
    PatternTooLong, ///< pattern: a wildcard pattern longer than `kMaxPatternLength`
    TrailingSlash, ///< address: a final '/', including the bare "/"
    EmptyPart, ///< address: two adjacent slashes
    IllegalByte, ///< address: a byte outside printable ASCII or one of " #*,?[]{}"
    PartTooLong ///< address: a part longer than `kMaxAddressPartLength`
};

/// The enumerator's name, for diagnostics.
constexpr const char* toString(Error error) noexcept
{
    switch (error)
    {
    case Error::MissingLeadingSlash:
        return "MissingLeadingSlash";
    case Error::UnterminatedClass:
        return "UnterminatedClass";
    case Error::UnterminatedBraces:
        return "UnterminatedBraces";
    case Error::PatternTooLong:
        return "PatternTooLong";
    case Error::TrailingSlash:
        return "TrailingSlash";
    case Error::EmptyPart:
        return "EmptyPart";
    case Error::IllegalByte:
        return "IllegalByte";
    case Error::PartTooLong:
        return "PartTooLong";
    }
    return "";
}

}
