/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace oscpm::detail
{

constexpr bool hasIllegalByte(std::string_view text) noexcept
{
    for (const char byte : text)
    {
        if (!isLegalInPattern(byte))
        {
            return true;
        }
    }
    return false;
}

struct ConstructScan
{
    std::size_t close;
    bool legal;
};

constexpr ConstructScan scanConstruct(std::string_view pattern, std::size_t open, char closeByte) noexcept
{
    bool legal = true;
    for (std::size_t i = open + 1; i < pattern.size(); ++i)
    {
        const char byte = pattern[i];
        if (byte == closeByte)
        {
            return { i, legal };
        }
        if (byte == k::partSeparator)
        {
            break;
        }
        if (!isLegalInPattern(byte))
        {
            legal = false;
        }
    }
    return { npos, legal };
}

static_assert(kMaxPatternLength <= kMaxAddressPartLength, "a part over the address part limit fits only in a pattern over the wildcard limit");

struct PatternScan
{
    std::optional<PatternError> fault;
    bool isLiteral = false;
};

constexpr PatternScan scanPattern(std::string_view pattern) noexcept
{
    if (!hasLeadingSlash(pattern))
    {
        return { PatternError::MissingLeadingSlash, false };
    }
    const std::size_t checked = pattern.size() < kMaxPatternLength ? pattern.size() : kMaxPatternLength;
    bool isLiteral = true;
    std::size_t i = 1;
    while (i < checked)
    {
        const PatternByte kind = classifyPatternByte(pattern[i]);
        if (kind == PatternByte::Plain)
        {
            ++i;
        }
        else if (kind == PatternByte::Separator)
        {
            isLiteral = isLiteral && pattern[i - 1] != k::partSeparator;
            ++i;
        }
        else if (kind == PatternByte::Wildcard)
        {
            isLiteral = false;
            ++i;
        }
        else if (kind == PatternByte::Illegal)
        {
            return { PatternError::IllegalByte, false };
        }
        else
        {
            const bool isClass = kind == PatternByte::SetOpen;
            const ConstructScan scan = scanConstruct(pattern, i, isClass ? k::setClose : k::listClose);
            if (scan.close == npos)
            {
                return { isClass ? PatternError::UnterminatedClass : PatternError::UnterminatedBraces, false };
            }
            if (!scan.legal)
            {
                return { PatternError::IllegalByte, false };
            }
            isLiteral = false;
            i = scan.close + 1;
        }
    }
    if (pattern.size() > kMaxPatternLength)
    {
        if (hasWildcard(pattern))
        {
            return { PatternError::PatternTooLong, false };
        }
        if (hasIllegalByte(pattern.substr(checked)))
        {
            return { PatternError::IllegalByte, false };
        }
        isLiteral = !hasPartLongerThan(pattern, kMaxAddressPartLength);
    }
    return { std::nullopt, isLiteral };
}

constexpr std::optional<AddressError> validateAddress(std::string_view address) noexcept
{
    if (!hasLeadingSlash(address))
    {
        return AddressError::MissingLeadingSlash;
    }
    std::size_t partLength = 0;
    for (std::size_t i = 1; i <= address.size(); ++i)
    {
        const bool atEnd = i == address.size();
        if (atEnd || address[i] == k::partSeparator)
        {
            if (partLength == 0)
            {
                return atEnd ? AddressError::TrailingSlash : AddressError::EmptyPart;
            }
            partLength = 0;
        }
        else if (!isLegalInAddressPart(address[i]))
        {
            return AddressError::IllegalByte;
        }
        else if (++partLength > kMaxAddressPartLength)
        {
            return AddressError::PartTooLong;
        }
    }
    return std::nullopt;
}

}
