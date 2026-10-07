/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/match.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/detail/validate.h>
#include <oscpm/error.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace oscpm
{

/// A validated address pattern. Holds a view of the caller's bytes, which
/// must outlive every use and stay unchanged; copying the value copies the
/// view.
class Pattern
{
public:
    class ParseResult;

    /// Parses `text`, yielding the pattern or the first fault.
    static constexpr ParseResult parse(std::string_view text) noexcept;

    /// Whether this pattern matches `address`, which is compared byte for
    /// byte and never validated.
    constexpr bool matches(std::string_view address) const noexcept
    {
        return m_isLiteral ? address == m_text : detail::matchParsed(m_text, address);
    }

    /// The bytes this pattern was parsed from.
    constexpr std::string_view text() const noexcept
    {
        return m_text;
    }

    /// Whether the pattern holds no wildcard, class, brace list or "//", so
    /// that it matches only the address equal to its text.
    constexpr bool isLiteral() const noexcept
    {
        return m_isLiteral;
    }

private:
    constexpr Pattern() noexcept = default;

    constexpr explicit Pattern(std::string_view text) noexcept
        : m_text(text)
        , m_isLiteral(detail::isLiteralText(text))
    {
    }

    std::string_view m_text;
    bool m_isLiteral = false;
};

/// A `Pattern` or the `Error` that stopped it parsing.
class Pattern::ParseResult
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
    constexpr Error error() const noexcept
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

    constexpr explicit ParseResult(Error error) noexcept
        : m_error(error)
    {
    }

    Pattern m_pattern;
    Error m_error = Error::MissingLeadingSlash;
    bool m_parsed = false;
};

constexpr Pattern::ParseResult Pattern::parse(std::string_view text) noexcept
{
    const std::optional<Error> fault = detail::validatePattern(text);
    return fault.has_value() ? ParseResult(*fault) : ParseResult(Pattern(text));
}

/// @return true if @p address matches @p pattern
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    const Pattern::ParseResult parsed = Pattern::parse(pattern);
    return parsed && parsed.pattern().matches(address);
}

}
