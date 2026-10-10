/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/address.h>
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

    /// Whether this pattern matches `address`.
    constexpr bool matches(const Address& address) const noexcept
    {
        const std::string_view text = address.text();
        return m_isLiteral ? text == m_text : detail::matchParsed(m_text, text);
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

/// @return true if @p address matches @p pattern; false when either is
/// malformed
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    const Expected<Pattern, PatternError> parsedPattern = Pattern::parse(pattern);
    if (!parsedPattern)
    {
        return false;
    }
    const Expected<Address, AddressError> parsedAddress = Address::parse(address);
    return parsedAddress && parsedPattern->matches(*parsedAddress);
}

}
