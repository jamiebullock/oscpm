/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace oscpm
{

/// The longest address part `match` compares. A longer address part never
/// matches, and `isValidAddress` rejects it.
constexpr std::size_t kMaxAddressPartLength = 4095;

}

namespace oscpm::detail
{

namespace k
{
    constexpr char partSeparator = '/';
    constexpr char anyByte = '?';
    constexpr char anyBytes = '*';
    constexpr char setOpen = '[';
    constexpr char setClose = ']';
    constexpr char setNegate = '!';
    constexpr char rangeSeparator = '-';
    constexpr char listOpen = '{';
    constexpr char listClose = '}';
    constexpr char listSeparator = ',';
    constexpr std::string_view reservedInAddress = "#*,?[]{}";
    constexpr unsigned char firstPrintableAscii = 0x21;
    constexpr unsigned char lastPrintableAscii = 0x7E;
    constexpr std::size_t bitsPerWord = 64;
    constexpr std::size_t reachWords = kMaxAddressPartLength / bitsPerWord + 1;
}

constexpr std::size_t npos = std::string_view::npos;

class Reach
{
public:
    constexpr explicit Reach(std::size_t partLength) noexcept
        : m_numWords(partLength / k::bitsPerWord + 1)
    {
    }

    constexpr bool test(std::size_t position) const noexcept
    {
        return ((m_words[position / k::bitsPerWord] >> (position % k::bitsPerWord)) & 1U) != 0U;
    }

    constexpr void set(std::size_t position) noexcept
    {
        m_words[position / k::bitsPerWord] |= std::uint64_t { 1 } << (position % k::bitsPerWord);
    }

    constexpr void setFrom(std::size_t first, std::size_t last) noexcept
    {
        for (std::size_t position = first; position <= last; ++position)
        {
            set(position);
        }
    }

    constexpr bool empty() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                return false;
            }
        }
        return true;
    }

    constexpr std::size_t lowest() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                std::size_t position = word * k::bitsPerWord;
                while (!test(position))
                {
                    ++position;
                }
                return position;
            }
        }
        return npos;
    }

private:
    std::array<std::uint64_t, k::reachWords> m_words { };
    std::size_t m_numWords;
};

constexpr bool startsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

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

constexpr bool matchPart(std::string_view pattern, std::string_view part) noexcept
{
    if (part.size() > kMaxAddressPartLength)
    {
        return false;
    }
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
            if (close == npos || pattern.find(k::listOpen, i + 1) < close)
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

class PartCursor
{
public:
    constexpr explicit PartCursor(std::string_view text) noexcept
        : m_text(text)
    {
    }

    constexpr bool exhausted() const noexcept
    {
        return m_exhausted;
    }

    constexpr std::string_view current() const noexcept
    {
        return m_text.substr(m_position, endOfCurrent() - m_position);
    }

    constexpr bool isOperator() const noexcept
    {
        return m_index > 0 && !isLast() && current().empty();
    }

    constexpr void advance() noexcept
    {
        if (isLast())
        {
            m_exhausted = true;
        }
        else
        {
            m_position = endOfCurrent() + 1;
        }
        ++m_index;
    }

private:
    constexpr std::size_t endOfCurrent() const noexcept
    {
        const std::size_t separator = m_text.find(k::partSeparator, m_position);
        return separator == npos ? m_text.size() : separator;
    }

    constexpr bool isLast() const noexcept
    {
        return endOfCurrent() == m_text.size();
    }

    std::string_view m_text;
    std::size_t m_position = 0;
    std::size_t m_index = 0;
    bool m_exhausted = false;
};

constexpr std::size_t closeWithinPart(std::string_view pattern, std::size_t open, char closeByte) noexcept
{
    const std::size_t close = pattern.find(closeByte, open + 1);
    const std::size_t nextSeparator = pattern.find(k::partSeparator, open + 1);
    return close < nextSeparator ? close : npos;
}

constexpr bool isPrintableAscii(char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    return value >= k::firstPrintableAscii && value <= k::lastPrintableAscii;
}

constexpr bool isReservedInAddress(char byte) noexcept
{
    return k::reservedInAddress.find(byte) != npos;
}

}

namespace oscpm
{

/// Whether `pattern` matches `address`: the OSC 1.0 rules for '?', '*',
/// '[...]' and '{a,b}', applied part by part, plus the OSC 1.1 '//'
/// operator matching zero or more whole parts. Bytes are compared as they
/// are and nothing is validated; a malformed pattern matches nothing.
/// Allocation-free, with running time bounded by the product of the two
/// lengths.
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    detail::PartCursor patternPart(pattern);
    detail::PartCursor addressPart(address);
    detail::PartCursor patternAfterOperator = patternPart;
    detail::PartCursor addressAtOperator = addressPart;
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
        if (!patternPart.exhausted() && !addressPart.exhausted() && detail::matchPart(patternPart.current(), addressPart.current()))
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

/// Whether `address` is a well-formed OSC address: it starts with '/', every
/// part is non-empty and no longer than `kMaxAddressPartLength`, and every
/// byte is printable ASCII other than '#', '*', ',', '?', '[', ']', '{'
/// and '}'.
constexpr bool isValidAddress(std::string_view address) noexcept
{
    if (address.empty() || address[0] != detail::k::partSeparator)
    {
        return false;
    }
    std::size_t partLength = 0;
    for (std::size_t i = 1; i <= address.size(); ++i)
    {
        if (i == address.size() || address[i] == detail::k::partSeparator)
        {
            if (partLength == 0 || partLength > kMaxAddressPartLength)
            {
                return false;
            }
            partLength = 0;
        }
        else if (!detail::isPrintableAscii(address[i]) || detail::isReservedInAddress(address[i]))
        {
            return false;
        }
        else
        {
            ++partLength;
        }
    }
    return true;
}

/// Whether `pattern` is a well-formed OSC address pattern: it starts with
/// '/', every byte is printable ASCII, every '[' and '{' is closed within
/// its part, and no '{' appears inside a brace list. Every pattern this
/// rejects matches nothing.
constexpr bool isValidPattern(std::string_view pattern) noexcept
{
    if (pattern.empty() || pattern[0] != detail::k::partSeparator)
    {
        return false;
    }
    for (const char byte : pattern)
    {
        if (!detail::isPrintableAscii(byte))
        {
            return false;
        }
    }
    std::size_t i = 0;
    while (i < pattern.size())
    {
        if (pattern[i] == detail::k::setOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::setClose);
            if (close == detail::npos)
            {
                return false;
            }
            i = close + 1;
        }
        else if (pattern[i] == detail::k::listOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::listClose);
            if (close == detail::npos || pattern.find(detail::k::listOpen, i + 1) < close)
            {
                return false;
            }
            i = close + 1;
        }
        else
        {
            ++i;
        }
    }
    return true;
}

}
