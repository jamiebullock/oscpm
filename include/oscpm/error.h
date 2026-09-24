/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>

namespace oscpm
{

/// The longest address part `match` compares. A longer address part never
/// matches, and `validateAddress` reports it as `PartTooLong`.
constexpr std::size_t kMaxAddressPartLength = 4095;

/// The longest pattern containing '*', '?', '[', '{' or "//" that
/// `validatePattern` accepts. A longer such pattern is reported as
/// `PatternTooLong` and matches nothing; a literal pattern has no limit.
constexpr std::size_t kMaxPatternLength = 1024;

/// A fault in a pattern, in an address or in an `AddressSpace` operation
/// (the last two). `MissingLeadingSlash` is reported for both a pattern
/// and an address; `UnterminatedClass`, `UnterminatedBraces` and
/// `PatternTooLong` only for a pattern; the four that follow them only for an
/// address.
enum class Error
{
    MissingLeadingSlash,
    UnterminatedClass,
    UnterminatedBraces,
    PatternTooLong,
    TrailingSlash,
    EmptyPart,
    IllegalByte,
    PartTooLong,
    Duplicate,
    NotFound
};

/// A fault and the zero-based byte offset at which it was found.
struct ParseError
{
    Error kind;
    std::size_t offset;
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
