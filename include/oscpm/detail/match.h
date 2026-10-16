/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/reach.h>
#include <oscpm/detail/string_view.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <cstddef>

namespace oscpm::detail
{

constexpr bool setContains(StringView members, char byte) noexcept
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
constexpr Reach<NumWords> advanceWhere(const Reach<NumWords>& reach, StringView part, Predicate matches) noexcept
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
constexpr Reach<NumWords> advanceByAlternatives(const Reach<NumWords>& reach, StringView part, StringView list) noexcept
{
    Reach<NumWords> next(part.size());
    StringView remaining = list;
    bool moreAlternatives = true;
    while (moreAlternatives)
    {
        const std::size_t comma = remaining.find(k::listSeparator);
        const StringView alternative = remaining.substr(0, comma);
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
constexpr bool matchPartWithin(StringView pattern, StringView part) noexcept
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
            const StringView members = pattern.substr(start, close - start);
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

struct PartShape
{
    bool isDirect = true;
    std::size_t star = npos;
    std::size_t numTokensBeforeStar = 0;
    std::size_t numTokensAfterStar = 0;
};

constexpr PartShape shapeOf(StringView pattern) noexcept
{
    PartShape shape;
    std::size_t i = 0;
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
            const std::size_t close = pattern.find(k::setClose, i + 1);
            if (close == npos)
            {
                shape.isDirect = false;
                return shape;
            }
            i = close + 1;
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
        ++(shape.star == npos ? shape.numTokensBeforeStar : shape.numTokensAfterStar);
    }
    return shape;
}

constexpr bool matchTokens(StringView tokens, StringView text) noexcept
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
            if (setContains(tokens.substr(start, close - start), candidate) == negate)
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

constexpr bool matchPartDirect(StringView pattern, StringView part, const PartShape& shape) noexcept
{
    const std::size_t numFixed = shape.numTokensBeforeStar + shape.numTokensAfterStar;
    if (shape.star == npos ? part.size() != numFixed : part.size() < numFixed)
    {
        return false;
    }
    const StringView prefix = pattern.substr(0, shape.star);
    const StringView suffix = shape.star == npos ? StringView() : pattern.substr(shape.star + 1);
    return matchTokens(prefix, part.substr(0, shape.numTokensBeforeStar))
        && matchTokens(suffix, part.substr(part.size() - shape.numTokensAfterStar));
}

constexpr bool matchPart(StringView pattern, StringView part) noexcept
{
    if (part.size() > kMaxAddressPartLength)
    {
        return false;
    }
    if (!hasOpener(pattern))
    {
        return pattern == part;
    }
    const PartShape shape = shapeOf(pattern);
    if (shape.isDirect)
    {
        return matchPartDirect(pattern, part, shape);
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
    constexpr explicit PatternCursor(StringView text) noexcept
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

    constexpr StringView part() const noexcept
    {
        return m_text.substr(m_partStart, m_partEnd - m_partStart);
    }

    constexpr bool matches(StringView addressPart) const noexcept
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

    StringView m_text;
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
    constexpr explicit AddressCursor(StringView text) noexcept
        : m_text(text)
    {
        readPart();
    }

    constexpr bool exhausted() const noexcept
    {
        return m_partStart > m_text.size();
    }

    constexpr StringView part() const noexcept
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

    StringView m_text;
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

struct PartSpan
{
    std::size_t patternEnd = 0;
    std::size_t addressEnd = 0;
};

constexpr std::size_t partEnd(StringView text, std::size_t from) noexcept
{
    const std::size_t separator = text.find(k::partSeparator, from);
    return separator == npos ? text.size() : separator;
}

constexpr bool matchPartAt(StringView pattern, std::size_t patternStart, StringView address, std::size_t addressStart, PartSpan& span) noexcept
{
    std::size_t i = patternStart;
    std::size_t j = addressStart;
    while (i <= pattern.size())
    {
        const char patternByte = i < pattern.size() ? pattern[i] : k::partSeparator;
        if (isOpener(patternByte))
        {
            span = { partEnd(pattern, i), partEnd(address, j) };
            return matchPart(pattern.substr(patternStart, span.patternEnd - patternStart), address.substr(addressStart, span.addressEnd - addressStart));
        }
        const char addressByte = j < address.size() ? address[j] : k::partSeparator;
        if (patternByte == k::partSeparator)
        {
            span = { i, j };
            return addressByte == k::partSeparator && j - addressStart <= kMaxAddressPartLength;
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

constexpr bool matchParsed(StringView pattern, StringView address) noexcept
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

constexpr bool matchWithCursors(StringView pattern, StringView address) noexcept
{
    return hasLeadingSlash(address) && matchParts(PatternCursor(pattern), AddressCursor(address));
}

}
