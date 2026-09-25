/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/error.h>

#include <cstddef>
#include <string_view>

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
    constexpr std::size_t operatorRunLength = 2;
    constexpr std::string_view descendantOperator = "//";
}

constexpr std::size_t npos = std::string_view::npos;

constexpr bool startsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

constexpr bool isOpener(char byte) noexcept
{
    return byte == k::anyBytes || byte == k::anyByte || byte == k::setOpen || byte == k::listOpen;
}

constexpr bool hasOpener(std::string_view text) noexcept
{
    for (const char byte : text)
    {
        if (isOpener(byte))
        {
            return true;
        }
    }
    return false;
}

constexpr std::size_t closeWithinPart(std::string_view pattern, std::size_t open, char closeByte) noexcept
{
    const std::size_t close = pattern.find(closeByte, open + 1);
    if (close == npos || pattern.substr(open + 1, close - open - 1).find(k::partSeparator) != npos)
    {
        return npos;
    }
    return close;
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

constexpr bool hasLeadingSlash(std::string_view text) noexcept
{
    return !text.empty() && text[0] == k::partSeparator;
}

constexpr bool hasWildcard(std::string_view pattern) noexcept
{
    return hasOpener(pattern) || pattern.find(k::descendantOperator) != npos;
}

constexpr bool isLiteralText(std::string_view pattern) noexcept
{
    std::size_t partLength = 0;
    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        const char byte = pattern[i];
        if (isOpener(byte))
        {
            return false;
        }
        if (byte == k::partSeparator)
        {
            if (i + 1 < pattern.size() && pattern[i + 1] == k::partSeparator)
            {
                return false;
            }
            partLength = 0;
        }
        else if (++partLength > kMaxAddressPartLength)
        {
            return false;
        }
    }
    return true;
}

}
