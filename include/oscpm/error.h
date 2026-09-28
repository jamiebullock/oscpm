/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>

namespace oscpm
{

/// The longest address part that matches; `AddressSpace::add` reports a
/// longer one as `PartTooLong`.
constexpr std::size_t kMaxAddressPartLength = 4095;

/// The longest pattern containing a wildcard, class, brace list or "//" that
/// `Pattern::parse` accepts; a longer one is `PatternTooLong`. A literal
/// pattern has no limit.
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
    PartTooLong, ///< address: a part longer than `kMaxAddressPartLength`
    Duplicate, ///< `AddressSpace::add`: the address is already registered
    NotFound ///< `AddressSpace::remove`: the address is not registered
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
    case Error::Duplicate:
        return "Duplicate";
    case Error::NotFound:
        return "NotFound";
    }
    return "";
}

}
