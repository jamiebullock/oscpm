/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/match.h>
#include <oscpm/detail/syntax.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace oscpm::detail
{

namespace k
{
    constexpr std::size_t maxPreparedParts = 64;
}

struct PreparedPart
{
    std::string_view text;
    bool isOperator = false;
    bool isLiteral = false;
};

using PreparedParts = std::array<PreparedPart, k::maxPreparedParts>;

inline std::size_t prepareParts(std::string_view pattern, PreparedParts& parts) noexcept
{
    std::size_t numParts = 0;
    for (PatternCursor cursor(pattern); !cursor.exhausted(); cursor.advance())
    {
        if (numParts == parts.size())
        {
            return npos;
        }
        PreparedPart& part = parts[numParts++];
        part.isOperator = cursor.isOperator();
        part.text = part.isOperator ? std::string_view() : cursor.part();
        part.isLiteral = !part.isOperator && !hasOpener(part.text);
    }
    return numParts;
}

class PreparedCursor
{
public:
    PreparedCursor(const PreparedParts& parts, std::size_t numParts) noexcept
        : m_parts(parts.data())
        , m_numParts(numParts)
    {
    }

    bool exhausted() const noexcept
    {
        return m_index == m_numParts;
    }

    bool isOperator() const noexcept
    {
        return m_parts[m_index].isOperator;
    }

    bool matches(std::string_view addressPart) const noexcept
    {
        const PreparedPart& part = m_parts[m_index];
        return part.isLiteral ? part.text == addressPart : matchPart(part.text, addressPart);
    }

    void advance() noexcept
    {
        ++m_index;
    }

private:
    const PreparedPart* m_parts;
    std::size_t m_numParts;
    std::size_t m_index = 0;
};

inline std::vector<std::size_t> partEndsOf(std::string_view address)
{
    std::vector<std::size_t> ends;
    ends.reserve(static_cast<std::size_t>(std::count(address.begin() + 1, address.end(), k::partSeparator)) + 1);
    for (std::size_t i = 1; i < address.size(); ++i)
    {
        if (address[i] == k::partSeparator)
        {
            ends.push_back(i);
        }
    }
    ends.push_back(address.size());
    return ends;
}

class StoredAddressCursor
{
public:
    StoredAddressCursor(std::string_view address, const std::vector<std::size_t>& partEnds) noexcept
        : m_address(address)
        , m_partEnds(partEnds.data())
        , m_numParts(partEnds.size())
    {
    }

    bool exhausted() const noexcept
    {
        return m_index == m_numParts;
    }

    std::string_view part() const noexcept
    {
        const std::size_t start = m_index == 0 ? 1 : m_partEnds[m_index - 1] + 1;
        return m_address.substr(start, m_partEnds[m_index] - start);
    }

    void advance() noexcept
    {
        ++m_index;
    }

private:
    std::string_view m_address;
    const std::size_t* m_partEnds;
    std::size_t m_numParts;
    std::size_t m_index = 0;
};

}
