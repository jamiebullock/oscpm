/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/match.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>

namespace oscpm::detail
{

namespace k
{
    constexpr std::size_t maxPreparedParts = 64;
    constexpr char noFurtherPart = 0;
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

class StoredAddress
{
public:
    /// Copies `address`, which must be well formed; allocates.
    explicit StoredAddress(std::string_view address)
        : m_bytes(new char[address.size() + numPartsOf(address) + 1])
        , m_addressLength(address.size())
    {
        std::copy(address.begin(), address.end(), m_bytes.get());
        char* partLength = m_bytes.get() + m_addressLength;
        std::size_t partStart = 1;
        for (std::size_t i = 1; i <= m_addressLength; ++i)
        {
            if (i == m_addressLength || address[i] == k::partSeparator)
            {
                *partLength++ = static_cast<char>(static_cast<unsigned char>(i - partStart));
                partStart = i + 1;
            }
        }
        *partLength = k::noFurtherPart;
    }

    StoredAddress(const StoredAddress& other)
        : StoredAddress(std::string_view(other))
    {
    }

    StoredAddress(StoredAddress&&) noexcept = default;
    StoredAddress& operator=(StoredAddress&&) noexcept = default;

    operator std::string_view() const noexcept
    {
        return std::string_view(m_bytes.get(), m_addressLength);
    }

private:
    friend class StoredAddressCursor;

    static std::size_t numPartsOf(std::string_view address) noexcept
    {
        return static_cast<std::size_t>(std::count(address.begin() + 1, address.end(), k::partSeparator)) + 1;
    }

    static_assert(kMaxAddressPartLength <= std::numeric_limits<unsigned char>::max(), "StoredAddress keeps the length of a part in one byte");

    // The address, then the length of each part in one byte, then `k::noFurtherPart`. No part is empty, so no length equals it.
    std::unique_ptr<char[]> m_bytes;
    std::size_t m_addressLength;
};

class StoredAddressCursor
{
public:
    explicit StoredAddressCursor(const StoredAddress& address) noexcept
        : m_part(address.m_bytes.get() + 1)
        , m_partLength(address.m_bytes.get() + address.m_addressLength)
    {
    }

    bool exhausted() const noexcept
    {
        return *m_partLength == k::noFurtherPart;
    }

    std::string_view part() const noexcept
    {
        return std::string_view(m_part, numPartBytes());
    }

    void advance() noexcept
    {
        m_part += numPartBytes() + 1;
        ++m_partLength;
    }

private:
    std::size_t numPartBytes() const noexcept
    {
        return static_cast<unsigned char>(*m_partLength);
    }

    const char* m_part;
    const char* m_partLength;
};

}
