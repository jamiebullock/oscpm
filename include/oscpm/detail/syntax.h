/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE < 8
#error "oscpm needs libstdc++ 8 or later: its std::string_view must be usable in constant expressions"
#endif

namespace oscpm::detail
{

namespace k
{
    constexpr char partSeparator = '/';
    constexpr char hash = '#';
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
    constexpr char nul = '\0';
    constexpr unsigned char firstPrintableAscii = 0x21;
    constexpr unsigned char lastPrintableAscii = 0x7E;
    constexpr unsigned char lastAscii = 0x7F;
    constexpr std::size_t operatorRunLength = 2;
    constexpr std::string_view descendantOperator = "//";
    constexpr std::size_t numByteValues = 256;
}

constexpr std::size_t npos = std::string_view::npos;

/// The `count` bytes of `text` from `from` on. `from + count` must not exceed
/// `text.size()`.
constexpr std::string_view slice(std::string_view text, std::size_t from, std::size_t count) noexcept
{
    return std::string_view(text.data() + from, count);
}

/// The bytes of `text` from `from` on. `from` must not exceed `text.size()`.
constexpr std::string_view sliceFrom(std::string_view text, std::size_t from) noexcept
{
    return slice(text, from, text.size() - from);
}

constexpr bool startsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && slice(text, 0, prefix.size()) == prefix;
}

constexpr bool isOpener(char byte) noexcept
{
    return byte == k::anyBytes || byte == k::anyByte || byte == k::setOpen || byte == k::listOpen;
}

constexpr std::size_t firstOpener(std::string_view text) noexcept
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (isOpener(text[i]))
        {
            return i;
        }
    }
    return npos;
}

constexpr bool hasOpener(std::string_view text) noexcept
{
    return firstOpener(text) != npos;
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

constexpr bool hasLeadingSlash(std::string_view text) noexcept
{
    return !text.empty() && text[0] == k::partSeparator;
}

constexpr bool hasWildcard(std::string_view pattern) noexcept
{
    return hasOpener(pattern) || pattern.find(k::descendantOperator) != npos;
}

}
