/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/string_view.h>
#include <oscpm/error.h>

#include <array>
#include <cstddef>

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
    constexpr StringView reservedInAddress = "#*,?[]{}";
    constexpr char nul = '\0';
    constexpr unsigned char firstPrintableAscii = 0x21;
    constexpr unsigned char lastPrintableAscii = 0x7E;
    constexpr unsigned char lastAscii = 0x7F;
    constexpr std::size_t operatorRunLength = 2;
    constexpr StringView descendantOperator = "//";
    constexpr std::size_t numByteValues = 256;
}

constexpr std::size_t npos = StringView::npos;

constexpr bool startsWith(StringView text, StringView prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

constexpr bool isOpener(char byte) noexcept
{
    return byte == k::anyBytes || byte == k::anyByte || byte == k::setOpen || byte == k::listOpen;
}

constexpr bool hasOpener(StringView text) noexcept
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

constexpr bool isPrintableAscii(char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    return value >= k::firstPrintableAscii && value <= k::lastPrintableAscii;
}

constexpr bool isReservedInAddress(char byte) noexcept
{
    return k::reservedInAddress.find(byte) != npos;
}

constexpr bool isLegalInPattern(char byte) noexcept
{
    return byte != k::nul && static_cast<unsigned char>(byte) <= k::lastAscii;
}

enum class PatternByte : unsigned char
{
    Plain,
    Separator,
    Wildcard,
    SetOpen,
    ListOpen,
    Illegal
};

using PatternByteTable = std::array<PatternByte, k::numByteValues>;

constexpr PatternByteTable patternByteTable() noexcept
{
    PatternByteTable table { };
    for (std::size_t value = 0; value < table.size(); ++value)
    {
        const auto byte = static_cast<char>(static_cast<unsigned char>(value));
        table[value] = !isLegalInPattern(byte)          ? PatternByte::Illegal
            : byte == k::partSeparator                  ? PatternByte::Separator
            : byte == k::anyBytes || byte == k::anyByte ? PatternByte::Wildcard
            : byte == k::setOpen                        ? PatternByte::SetOpen
            : byte == k::listOpen                       ? PatternByte::ListOpen
                                                        : PatternByte::Plain;
    }
    return table;
}

namespace k
{
    inline constexpr PatternByteTable patternByte = patternByteTable();
}

constexpr PatternByte classifyPatternByte(char byte) noexcept
{
    return k::patternByte[static_cast<unsigned char>(byte)];
}

using ByteTable = std::array<bool, k::numByteValues>;

constexpr ByteTable legalInAddressPartTable() noexcept
{
    ByteTable table { };
    for (std::size_t value = 0; value < table.size(); ++value)
    {
        const auto byte = static_cast<char>(static_cast<unsigned char>(value));
        table[value] = isPrintableAscii(byte) && !isReservedInAddress(byte);
    }
    return table;
}

namespace k
{
    inline constexpr ByteTable legalInAddressPart = legalInAddressPartTable();
}

constexpr bool isLegalInAddressPart(char byte) noexcept
{
    return k::legalInAddressPart[static_cast<unsigned char>(byte)];
}

constexpr bool hasLeadingSlash(StringView text) noexcept
{
    return !text.empty() && text[0] == k::partSeparator;
}

constexpr bool hasWildcard(StringView pattern) noexcept
{
    return hasOpener(pattern) || pattern.find(k::descendantOperator) != npos;
}

constexpr bool isLiteralText(StringView pattern) noexcept
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
