/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/match.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace oscpm
{

/// The first fault in `pattern` by byte offset, or nothing when it parses:
/// `MissingLeadingSlash` at 0, `UnterminatedClass` at a '[' with no ']'
/// before the next '/', `UnterminatedBraces` at a '{' with no '}' before the
/// next '/', `PatternTooLong` at `kMaxPatternLength` for a longer pattern
/// containing '*', '?', '[', '{' or "//".
/// Every other pattern parses; a byte no address can contain is a literal
/// that matches nothing where it stands.
constexpr std::optional<ParseError> validatePattern(std::string_view pattern) noexcept
{
    if (!detail::hasLeadingSlash(pattern))
    {
        return ParseError { Error::MissingLeadingSlash, 0 };
    }
    const std::size_t checked = pattern.size() < kMaxPatternLength ? pattern.size() : kMaxPatternLength;
    std::size_t i = 0;
    while (i < checked)
    {
        if (pattern[i] == detail::k::setOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::setClose);
            if (close == detail::npos)
            {
                return ParseError { Error::UnterminatedClass, i };
            }
            i = close + 1;
        }
        else if (pattern[i] == detail::k::listOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::listClose);
            if (close == detail::npos)
            {
                return ParseError { Error::UnterminatedBraces, i };
            }
            i = close + 1;
        }
        else
        {
            ++i;
        }
    }
    if (pattern.size() > kMaxPatternLength && detail::hasWildcard(pattern))
    {
        return ParseError { Error::PatternTooLong, kMaxPatternLength };
    }
    return std::nullopt;
}

/// The first fault in `address` by byte offset, or nothing when it is a
/// well-formed OSC address: `MissingLeadingSlash` at 0, `TrailingSlash` at
/// a final '/' (the bare "/" faults at 0), `EmptyPart` at the second of two
/// adjacent slashes, `IllegalByte` at a byte outside printable ASCII or one
/// of " #*,?[]{}", `PartTooLong` at the first byte of a part beyond
/// `kMaxAddressPartLength`.
constexpr std::optional<ParseError> validateAddress(std::string_view address) noexcept
{
    if (!detail::hasLeadingSlash(address))
    {
        return ParseError { Error::MissingLeadingSlash, 0 };
    }
    std::size_t partLength = 0;
    for (std::size_t i = 1; i <= address.size(); ++i)
    {
        const bool atEnd = i == address.size();
        if (atEnd || address[i] == detail::k::partSeparator)
        {
            if (partLength == 0)
            {
                return atEnd ? ParseError { Error::TrailingSlash, i - 1 } : ParseError { Error::EmptyPart, i };
            }
            partLength = 0;
        }
        else if (!detail::isPrintableAscii(address[i]) || detail::isReservedInAddress(address[i]))
        {
            return ParseError { Error::IllegalByte, i };
        }
        else if (++partLength > kMaxAddressPartLength)
        {
            return ParseError { Error::PartTooLong, i };
        }
    }
    return std::nullopt;
}

class ParseResult;

/// A validated address pattern. Holds a view of the caller's bytes, which
/// must outlive every use; copying the value copies the view.
class Pattern
{
public:
    /// Parses `text`, yielding the pattern or the first fault by byte offset
    /// as `validatePattern` reports it.
    static constexpr ParseResult parse(std::string_view text) noexcept;

    /// Whether this pattern matches `address`: the OSC 1.0 rules for '?',
    /// '*', '[...]' and '{a,b}', applied part by part, plus the OSC 1.1 '//'
    /// operator matching zero or more whole parts, which a run of two or
    /// more slashes anywhere in the pattern denotes. The address is compared
    /// byte for byte and never validated. Allocation-free, with running time
    /// bounded by the product of the two lengths.
    constexpr bool matches(std::string_view address) const noexcept
    {
        return m_isLiteral ? address == m_text : detail::matchParsed(m_text, address);
    }

    /// The bytes this pattern was parsed from.
    constexpr std::string_view text() const noexcept
    {
        return m_text;
    }

    /// Whether the pattern contains no '*', '?', '[' or '{', no run of two
    /// or more slashes and no part longer than `kMaxAddressPartLength`, so
    /// that it matches only an address equal to its text.
    constexpr bool isLiteral() const noexcept
    {
        return m_isLiteral;
    }

private:
    friend class ParseResult;

    constexpr Pattern() noexcept = default;

    constexpr explicit Pattern(std::string_view text) noexcept
        : m_text(text)
        , m_isLiteral(detail::isLiteralText(text))
    {
    }

    std::string_view m_text;
    bool m_isLiteral = false;
};

/// A `Pattern` or the `ParseError` that stopped it parsing.
class ParseResult
{
public:
    /// Whether parsing succeeded and `pattern()` holds the result.
    constexpr explicit operator bool() const noexcept
    {
        return m_parsed;
    }

    /// The pattern; meaningful only when the result is true.
    constexpr const Pattern& pattern() const noexcept
    {
        return m_pattern;
    }

    /// The fault; meaningful only when the result is false.
    constexpr const ParseError& error() const noexcept
    {
        return m_error;
    }

private:
    friend class Pattern;

    constexpr explicit ParseResult(Pattern pattern) noexcept
        : m_pattern(pattern)
        , m_parsed(true)
    {
    }

    constexpr explicit ParseResult(ParseError error) noexcept
        : m_error(error)
    {
    }

    Pattern m_pattern;
    ParseError m_error { Error::MissingLeadingSlash, 0 };
    bool m_parsed = false;
};

constexpr ParseResult Pattern::parse(std::string_view text) noexcept
{
    const std::optional<ParseError> fault = validatePattern(text);
    return fault.has_value() ? ParseResult(*fault) : ParseResult(Pattern(text));
}

/// Parses `pattern` and tests it against `address`, as `Pattern::parse`
/// followed by `Pattern::matches`. A malformed pattern matches nothing; use
/// `Pattern::parse` to learn why.
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    const ParseResult parsed = Pattern::parse(pattern);
    return parsed && parsed.pattern().matches(address);
}

}
