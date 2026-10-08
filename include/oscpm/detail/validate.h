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

constexpr std::optional<PatternError> validatePattern(std::string_view pattern) noexcept
{
    if (!hasLeadingSlash(pattern))
    {
        return PatternError::MissingLeadingSlash;
    }
    const std::size_t checked = pattern.size() < kMaxPatternLength ? pattern.size() : kMaxPatternLength;
    std::size_t i = 0;
    while (i < checked)
    {
        if (pattern[i] == k::setOpen)
        {
            const std::size_t close = closeWithinPart(pattern, i, k::setClose);
            if (close == npos)
            {
                return PatternError::UnterminatedClass;
            }
            i = close + 1;
        }
        else if (pattern[i] == k::listOpen)
        {
            const std::size_t close = closeWithinPart(pattern, i, k::listClose);
            if (close == npos)
            {
                return PatternError::UnterminatedBraces;
            }
            i = close + 1;
        }
        else
        {
            ++i;
        }
    }
    if (pattern.size() > kMaxPatternLength && hasWildcard(pattern))
    {
        return PatternError::PatternTooLong;
    }
    return std::nullopt;
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
        else if (!isPrintableAscii(address[i]) || isReservedInAddress(address[i]))
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
