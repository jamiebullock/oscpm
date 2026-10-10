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
#include <oscpm/expected.h>

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
    /// Parses `text`, yielding the pattern or the first fault.
    static constexpr Expected<Pattern, PatternError> parse(std::string_view text) noexcept;

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
    constexpr explicit Pattern(std::string_view text) noexcept
        : m_text(text)
        , m_isLiteral(detail::isLiteralText(text))
    {
    }

    std::string_view m_text;
    bool m_isLiteral;
};

constexpr Expected<Pattern, PatternError> Pattern::parse(std::string_view text) noexcept
{
    const std::optional<PatternError> fault = detail::validatePattern(text);
    if (fault.has_value())
    {
        return *fault;
    }
    return Pattern(text);
}

/// @return true if @p address matches @p pattern
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    const Expected<Pattern, PatternError> parsed = Pattern::parse(pattern);
    return parsed && parsed->matches(address);
}

}
