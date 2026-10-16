/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/syntax.h>
#include <oscpm/detail/word.h>
#include <oscpm/error.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace oscpm::detail
{

namespace k
{
    constexpr std::size_t firstByteRun = 4 * wordSize;
}

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

/// Whether `word` holds a byte that is not plain in a pattern: a null, a byte
/// above 0x7F or an opener.
constexpr bool hasPatternOpenerOrIllegalByte(Word word) noexcept
{
    const Word folded = word | broadcast(k::lowerCaseBit);
    const Word plain = bytesDifferingFrom(word, k::nul) & bytesDifferingFrom(word, k::anyBytes)
        & bytesDifferingFrom(word, k::anyByte) & bytesDifferingFrom(folded, k::listOpen);
    return ((~plain | bytesAboveAscii(word)) & k::highBitPerByte) != 0;
}

/// Whether `word` holds a byte an address part cannot: one outside printable
/// ASCII or one of `k::reservedInAddress`.
constexpr bool hasIllegalAddressByte(Word word) noexcept
{
    const Word folded = word | broadcast(k::lowerCaseBit);
    const Word printable = bytesAtLeast(word, k::firstPrintableAscii) & bytesDifferingFrom(word, k::lastAscii);
    const Word plain = printable & bytesDifferingFrom(word, k::hash) & bytesDifferingFrom(word, k::anyBytes)
        & bytesDifferingFrom(word, k::listSeparator) & bytesDifferingFrom(word, k::anyByte)
        & bytesDifferingFrom(folded, k::listOpen) & bytesDifferingFrom(folded, k::listClose);
    return ((~plain | bytesAboveAscii(word)) & k::highBitPerByte) != 0;
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
    std::size_t byteRun = k::firstByteRun;
    while (i < checked)
    {
        if (i + k::wordSize <= checked)
        {
            const Word word = loadWord(pattern, i);
            if (!hasPatternOpenerOrIllegalByte(word))
            {
                const Word separators = bytesEqualTo(word, k::partSeparator);
                const bool afterSeparator = pattern[i - 1] == k::partSeparator;
                isLiteral = isLiteral && !hasAdjacentPair(separators) && !(afterSeparator && hasFirstByte(separators));
                i += k::wordSize;
                byteRun = k::firstByteRun;
                continue;
            }
        }
        const std::size_t stop = i + byteRun < checked ? i + byteRun : checked;
        byteRun *= 2;
        while (i < stop)
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

/// Validates `address` from byte `from`, where the current part starts at
/// `partStart`; the bytes before `from` must already have passed.
constexpr std::optional<AddressError> validateAddressBytes(std::string_view address, std::size_t from, std::size_t partStart) noexcept
{
    for (std::size_t i = from; i < address.size(); ++i)
    {
        const char byte = address[i];
        if (byte == k::partSeparator)
        {
            if (i == partStart)
            {
                return AddressError::EmptyPart;
            }
            partStart = i + 1;
        }
        else if (!isLegalInAddressPart(byte))
        {
            return AddressError::IllegalByte;
        }
        else if (i + 1 - partStart > kMaxAddressPartLength)
        {
            return AddressError::PartTooLong;
        }
    }
    return partStart == address.size() ? std::optional<AddressError>(AddressError::TrailingSlash) : std::nullopt;
}

constexpr std::optional<AddressError> validateAddress(std::string_view address) noexcept
{
    if (!hasLeadingSlash(address))
    {
        return AddressError::MissingLeadingSlash;
    }
    std::size_t checked = 1;
    std::size_t partStart = 1;
    while (checked + k::wordSize <= address.size())
    {
        const Word word = loadWord(address, checked);
        if (hasIllegalAddressByte(word) || checked + k::wordSize - partStart > kMaxAddressPartLength)
        {
            break;
        }
        const Word separators = bytesEqualTo(word, k::partSeparator);
        if (hasAdjacentPair(separators) || (partStart == checked && hasFirstByte(separators)))
        {
            return AddressError::EmptyPart;
        }
        checked += k::wordSize;
        partStart = separators != 0 ? checked - numBytesAfterLast(separators) : partStart;
    }
    return validateAddressBytes(address, checked, partStart);
}

}
