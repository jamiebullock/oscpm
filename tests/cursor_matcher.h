/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/match.h>
#include <oscpm/detail/syntax.h>

#include <cstddef>
#include <string_view>

namespace oscpm_test
{

class MatchingPatternCursor : public oscpm::detail::PatternCursor
{
public:
    using oscpm::detail::PatternCursor::PatternCursor;

    constexpr bool matches(std::string_view addressPart) const noexcept
    {
        return oscpm::detail::matchPart(part(), addressPart);
    }
};

class AddressCursor
{
public:
    constexpr explicit AddressCursor(std::string_view text) noexcept
        : m_text(text)
    {
        readPart();
    }

    constexpr bool exhausted() const noexcept
    {
        return m_partStart > m_text.size();
    }

    constexpr std::string_view part() const noexcept
    {
        return m_text.substr(m_partStart, m_partEnd - m_partStart);
    }

    constexpr void advance() noexcept
    {
        m_partStart = m_partEnd + 1;
        readPart();
    }

private:
    constexpr void readPart() noexcept
    {
        if (exhausted())
        {
            return;
        }
        const std::size_t separator = m_text.find(oscpm::detail::k::partSeparator, m_partStart);
        m_partEnd = separator == std::string_view::npos ? m_text.size() : separator;
    }

    std::string_view m_text;
    std::size_t m_partStart = 1;
    std::size_t m_partEnd = 0;
};

/// Match a parsed pattern against a parsed address one part at a time, as a
/// second answer to compare with `oscpm::detail::matchParsed`.
constexpr bool matchWithCursors(std::string_view pattern, std::string_view address) noexcept
{
    return oscpm::detail::hasLeadingSlash(address)
        && oscpm::detail::matchParts(MatchingPatternCursor(pattern), AddressCursor(address));
}

}
