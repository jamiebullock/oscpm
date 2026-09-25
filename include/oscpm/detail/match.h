/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/reach.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <cstddef>
#include <string_view>

namespace oscpm::detail
{

constexpr bool setContains(std::string_view members, char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    std::size_t i = 0;
    while (i < members.size())
    {
        const auto low = static_cast<unsigned char>(members[i]);
        const bool isRange = i + 2 < members.size() && members[i + 1] == k::rangeSeparator;
        if (isRange)
        {
            const auto high = static_cast<unsigned char>(members[i + 2]);
            if (low <= value && value <= high)
            {
                return true;
            }
            i += 3;
        }
        else
        {
            if (low == value)
            {
                return true;
            }
            i += 1;
        }
    }
    return false;
}

template <std::size_t NumWords, typename Predicate>
constexpr Reach<NumWords> advanceWhere(const Reach<NumWords>& reach, std::string_view part, Predicate matches) noexcept
{
    Reach<NumWords> next(part.size());
    for (std::size_t position = 0; position < part.size(); ++position)
    {
        if (reach.test(position) && matches(part[position]))
        {
            next.set(position + 1);
        }
    }
    return next;
}

template <std::size_t NumWords>
constexpr Reach<NumWords> advanceByAlternatives(const Reach<NumWords>& reach, std::string_view part, std::string_view list) noexcept
{
    Reach<NumWords> next(part.size());
    std::string_view remaining = list;
    bool moreAlternatives = true;
    while (moreAlternatives)
    {
        const std::size_t comma = remaining.find(k::listSeparator);
        const std::string_view alternative = remaining.substr(0, comma);
        for (std::size_t position = 0; position <= part.size(); ++position)
        {
            if (reach.test(position) && startsWith(part.substr(position), alternative))
            {
                next.set(position + alternative.size());
            }
        }
        moreAlternatives = comma != npos;
        if (moreAlternatives)
        {
            remaining = remaining.substr(comma + 1);
        }
    }
    return next;
}

template <std::size_t NumWords>
constexpr bool matchPartWithin(std::string_view pattern, std::string_view part) noexcept
{
    Reach<NumWords> reach(part.size());
    reach.set(0);
    std::size_t i = 0;
    while (i < pattern.size() && !reach.empty())
    {
        const char byte = pattern[i];
        if (byte == k::anyBytes)
        {
            reach.setFrom(reach.lowest(), part.size());
            ++i;
        }
        else if (byte == k::anyByte)
        {
            reach = advanceWhere(reach, part, []([[maybe_unused]] char candidate)
                { return true; });
            ++i;
        }
        else if (byte == k::setOpen)
        {
            std::size_t start = i + 1;
            const bool negate = start < pattern.size() && pattern[start] == k::setNegate;
            if (negate)
            {
                ++start;
            }
            const std::size_t close = pattern.find(k::setClose, start);
            if (close == npos)
            {
                return false;
            }
            const std::string_view members = pattern.substr(start, close - start);
            reach = advanceWhere(reach, part, [members, negate](char candidate)
                { return setContains(members, candidate) != negate; });
            i = close + 1;
        }
        else if (byte == k::listOpen)
        {
            const std::size_t close = pattern.find(k::listClose, i + 1);
            if (close == npos)
            {
                return false;
            }
            reach = advanceByAlternatives(reach, part, pattern.substr(i + 1, close - i - 1));
            i = close + 1;
        }
        else
        {
            reach = advanceWhere(reach, part, [byte](char candidate)
                { return candidate == byte; });
            ++i;
        }
    }
    return reach.test(part.size());
}

constexpr bool matchPart(std::string_view pattern, std::string_view part) noexcept
{
    if (part.size() > kMaxAddressPartLength)
    {
        return false;
    }
    if (!hasOpener(pattern))
    {
        return pattern == part;
    }
    if (part.size() < k::bitsPerWord)
    {
        return matchPartWithin<k::smallReachWords>(pattern, part);
    }
    return matchPartWithin<k::reachWords>(pattern, part);
}

class PatternCursor
{
public:
    constexpr explicit PatternCursor(std::string_view text) noexcept
        : m_text(text)
    {
        readToken();
    }

    constexpr bool exhausted() const noexcept
    {
        return m_exhausted;
    }

    constexpr bool isOperator() const noexcept
    {
        return m_isOperator;
    }

    constexpr std::string_view part() const noexcept
    {
        return m_text.substr(m_partStart, m_partEnd - m_partStart);
    }

    constexpr bool matches(std::string_view addressPart) const noexcept
    {
        return matchPart(part(), addressPart);
    }

    constexpr void advance() noexcept
    {
        m_position = m_next;
        readToken();
    }

private:
    constexpr void readToken() noexcept
    {
        if (m_position >= m_text.size())
        {
            m_exhausted = true;
            return;
        }
        m_partStart = m_position;
        if (m_text[m_position] == k::partSeparator)
        {
            std::size_t runLength = 0;
            while (m_position + runLength < m_text.size() && m_text[m_position + runLength] == k::partSeparator)
            {
                ++runLength;
            }
            if (runLength >= k::operatorRunLength)
            {
                m_isOperator = true;
                m_next = m_position + runLength;
                return;
            }
            m_partStart = m_position + 1;
        }
        m_isOperator = false;
        const std::size_t separator = m_text.find(k::partSeparator, m_partStart);
        m_partEnd = separator == npos ? m_text.size() : separator;
        m_next = m_partEnd;
    }

    std::string_view m_text;
    std::size_t m_position = 0;
    std::size_t m_next = 0;
    std::size_t m_partStart = 0;
    std::size_t m_partEnd = 0;
    bool m_isOperator = false;
    bool m_exhausted = false;
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
        const std::size_t separator = m_text.find(k::partSeparator, m_partStart);
        m_partEnd = separator == npos ? m_text.size() : separator;
    }

    std::string_view m_text;
    std::size_t m_partStart = 1;
    std::size_t m_partEnd = 0;
};

template <typename PatternParts, typename AddressParts>
constexpr bool matchParts(PatternParts patternPart, AddressParts addressPart) noexcept
{
    PatternParts patternAfterOperator = patternPart;
    AddressParts addressAtOperator = addressPart;
    bool seenOperator = false;
    while (!patternPart.exhausted() || !addressPart.exhausted())
    {
        if (!patternPart.exhausted() && patternPart.isOperator())
        {
            patternPart.advance();
            patternAfterOperator = patternPart;
            addressAtOperator = addressPart;
            seenOperator = true;
            continue;
        }
        if (!patternPart.exhausted() && !addressPart.exhausted() && patternPart.matches(addressPart.part()))
        {
            patternPart.advance();
            addressPart.advance();
            continue;
        }
        if (!seenOperator || addressAtOperator.exhausted())
        {
            return false;
        }
        addressAtOperator.advance();
        patternPart = patternAfterOperator;
        addressPart = addressAtOperator;
    }
    return true;
}

constexpr bool matchParsed(std::string_view pattern, std::string_view address) noexcept
{
    return hasLeadingSlash(address) && matchParts(PatternCursor(pattern), AddressCursor(address));
}

}
