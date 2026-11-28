/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/compiler.h>
#include <oscpm/detail/reach.h>
#include <oscpm/detail/syntax.h>

#include <algorithm>
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

template <typename Predicate>
constexpr Reach advanceWhere(const Reach& reach, std::string_view part, Predicate matches) noexcept
{
    Reach next(part.size());
    for (std::size_t position = 0; position < part.size(); ++position)
    {
        if (reach.test(position) && matches(part[position]))
        {
            next.set(position + 1);
        }
    }
    return next;
}

constexpr Reach advanceByAlternatives(const Reach& reach, std::string_view part, std::string_view list) noexcept
{
    Reach next(part.size());
    std::string_view remaining = list;
    bool moreAlternatives = true;
    while (moreAlternatives)
    {
        const std::size_t comma = remaining.find(k::listSeparator);
        const std::string_view alternative = slice(remaining, 0, std::min(comma, remaining.size()));
        for (std::size_t position = 0; position <= part.size(); ++position)
        {
            if (reach.test(position) && startsWith(sliceFrom(part, position), alternative))
            {
                next.set(position + alternative.size());
            }
        }
        moreAlternatives = comma != npos;
        if (moreAlternatives)
        {
            remaining = sliceFrom(remaining, comma + 1);
        }
    }
    return next;
}

constexpr bool matchPartWithin(std::string_view pattern, std::string_view part) noexcept
{
    Reach reach(part.size());
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
            const bool negate = pattern[start] == k::setNegate;
            if (negate)
            {
                ++start;
            }
            const std::size_t close = pattern.find(k::setClose, start);
            const std::string_view members = slice(pattern, start, close - start);
            reach = advanceWhere(reach, part, [members, negate](char candidate)
                { return setContains(members, candidate) != negate; });
            i = close + 1;
        }
        else if (byte == k::listOpen)
        {
            const std::size_t close = pattern.find(k::listClose, i + 1);
            reach = advanceByAlternatives(reach, part, slice(pattern, i + 1, close - i - 1));
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

struct PartShape
{
    bool isDirect = true;
    bool exceedsPart = false;
    std::size_t star = npos;
    std::size_t numTokensBeforeStar = 0;
    std::size_t numTokensAfterStar = 0;
};

constexpr PartShape shapeOf(std::string_view pattern, std::size_t firstOpener, std::size_t maxTokens) noexcept
{
    PartShape shape;
    shape.numTokensBeforeStar = firstOpener;
    std::size_t numTokens = firstOpener;
    std::size_t i = firstOpener;
    while (i < pattern.size())
    {
        const char byte = pattern[i];
        if (byte == k::anyBytes)
        {
            if (shape.star != npos)
            {
                shape.isDirect = false;
                return shape;
            }
            shape.star = i;
            ++i;
            continue;
        }
        if (byte == k::setOpen)
        {
            i = pattern.find(k::setClose, i + 1) + 1;
        }
        else if (byte == k::listOpen)
        {
            shape.isDirect = false;
            return shape;
        }
        else
        {
            ++i;
        }
        if (++numTokens > maxTokens)
        {
            shape.exceedsPart = true;
            return shape;
        }
        ++(shape.star == npos ? shape.numTokensBeforeStar : shape.numTokensAfterStar);
    }
    return shape;
}

constexpr bool matchTokens(std::string_view tokens, std::string_view text) noexcept
{
    std::size_t i = 0;
    for (const char candidate : text)
    {
        const char byte = tokens[i];
        if (byte == k::setOpen)
        {
            std::size_t start = i + 1;
            const bool negate = tokens[start] == k::setNegate;
            if (negate)
            {
                ++start;
            }
            const std::size_t close = tokens.find(k::setClose, start);
            if (setContains(slice(tokens, start, close - start), candidate) == negate)
            {
                return false;
            }
            i = close + 1;
        }
        else
        {
            if (byte != k::anyByte && byte != candidate)
            {
                return false;
            }
            ++i;
        }
    }
    return true;
}

constexpr bool matchPartDirect(std::string_view pattern, std::string_view part, const PartShape& shape) noexcept
{
    const std::size_t numFixed = shape.numTokensBeforeStar + shape.numTokensAfterStar;
    if (shape.star == npos ? part.size() != numFixed : part.size() < numFixed)
    {
        return false;
    }
    const std::string_view prefix = slice(pattern, 0, std::min(shape.star, pattern.size()));
    const std::string_view suffix = shape.star == npos ? std::string_view() : sliceFrom(pattern, shape.star + 1);
    return matchTokens(prefix, slice(part, 0, shape.numTokensBeforeStar))
        && matchTokens(suffix, sliceFrom(part, part.size() - shape.numTokensAfterStar));
}

OSCPM_NOINLINE constexpr bool matchPart(std::string_view pattern, std::string_view part) noexcept
{
    const std::size_t opener = firstOpener(pattern);
    if (opener == npos)
    {
        return pattern == part;
    }
    const PartShape shape = shapeOf(pattern, opener, part.size());
    if (shape.exceedsPart)
    {
        return false;
    }
    if (shape.isDirect)
    {
        return matchPartDirect(pattern, part, shape);
    }
    return matchPartWithin(pattern, part);
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
        return slice(m_text, m_partStart, m_partEnd - m_partStart);
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

struct PartSpan
{
    std::size_t patternEnd = 0;
    std::size_t addressEnd = 0;
};

constexpr std::size_t partEnd(std::string_view text, std::size_t from) noexcept
{
    const std::size_t separator = text.find(k::partSeparator, from);
    return separator == npos ? text.size() : separator;
}

constexpr bool matchPartAt(std::string_view pattern, std::size_t patternStart, std::string_view address, std::size_t addressStart, PartSpan& span) noexcept
{
    std::size_t i = patternStart;
    std::size_t j = addressStart;
    while (i <= pattern.size())
    {
        const char patternByte = i < pattern.size() ? pattern[i] : k::partSeparator;
        if (isOpener(patternByte))
        {
            span = { partEnd(pattern, i), partEnd(address, j) };
            return matchPart(slice(pattern, patternStart, span.patternEnd - patternStart), slice(address, addressStart, span.addressEnd - addressStart));
        }
        const char addressByte = j < address.size() ? address[j] : k::partSeparator;
        if (patternByte == k::partSeparator)
        {
            span = { i, j };
            return addressByte == k::partSeparator;
        }
        if (patternByte != addressByte)
        {
            return false;
        }
        ++i;
        ++j;
    }
    return false;
}

constexpr bool matchParsed(std::string_view pattern, std::string_view address) noexcept
{
    if (!hasLeadingSlash(address))
    {
        return false;
    }
    const std::size_t patternSize = pattern.size();
    const std::size_t addressSize = address.size();
    std::size_t patternStart = pattern.empty() || hasLeadingSlash(pattern) ? 1 : 0;
    std::size_t addressStart = 1;
    std::size_t retryPattern = 0;
    std::size_t retryAddress = 0;
    bool seenOperator = false;
    while (patternStart <= patternSize || addressStart <= addressSize)
    {
        if (patternStart < patternSize && pattern[patternStart] == k::partSeparator)
        {
            while (patternStart < patternSize && pattern[patternStart] == k::partSeparator)
            {
                ++patternStart;
            }
            if (patternStart == patternSize)
            {
                ++patternStart;
            }
            retryPattern = patternStart;
            retryAddress = addressStart;
            seenOperator = true;
            continue;
        }
        PartSpan span;
        if (patternStart <= patternSize && addressStart <= addressSize && matchPartAt(pattern, patternStart, address, addressStart, span))
        {
            patternStart = span.patternEnd + 1;
            addressStart = span.addressEnd + 1;
            continue;
        }
        if (!seenOperator || retryAddress > addressSize)
        {
            return false;
        }
        retryAddress = partEnd(address, retryAddress) + 1;
        patternStart = retryPattern;
        addressStart = retryAddress;
    }
    return true;
}

}
