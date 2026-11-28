/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <optional>

namespace oscpm
{

/// The maximum length, in bytes, of one part of an address.
constexpr std::size_t kMaxAddressPartLength = 255;

/// The maximum length, in bytes, of a pattern containing a wildcard, class,
/// brace list or "//". A literal pattern has no limit.
constexpr std::size_t kMaxPatternLength = 1024;

/// The longest pattern whose matches are kept; a longer one is matched afresh
/// every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// A fault that stops a pattern parsing.
enum class PatternError
{
    MissingLeadingSlash, ///< no leading '/'
    UnterminatedClass, ///< a '[' with no ']' before the next '/'
    UnterminatedBraces, ///< a '{' with no '}' before the next '/'
    IllegalByte, ///< a null or a byte above 0x7F, which an OSC-string cannot hold
    PatternTooLong ///< a wildcard pattern longer than `kMaxPatternLength`
};

/// A fault that stops an address parsing.
enum class AddressError
{
    MissingLeadingSlash, ///< no leading '/'
    TrailingSlash, ///< a final '/', including the bare "/"
    EmptyPart, ///< two adjacent slashes
    IllegalByte, ///< a byte outside printable ASCII or one of " #*,?[]{}"
    PartTooLong ///< a part longer than `kMaxAddressPartLength`
};

/// The outcome of `AddressSpace::visit` and `AddressSpace::dispatch`.
struct MatchResult
{
    std::size_t matched = 0; ///< the number of values visited or called; 0 when `error` is set
    std::optional<PatternError> error; ///< the parse fault when the pattern was malformed and no value was reached
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
    case PatternError::IllegalByte:
        return "IllegalByte";
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
