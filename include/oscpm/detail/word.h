/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace oscpm::detail
{

/// Eight bytes of text, the first in the low byte.
using Word = std::uint64_t;

namespace k
{
    constexpr std::size_t wordSize = sizeof(Word);
    constexpr unsigned bitsPerByte = 8;
    constexpr Word onePerByte = 0x0101010101010101;
    constexpr Word highBitPerByte = 0x8080808080808080;
    constexpr Word lowBitsPerByte = 0x7F7F7F7F7F7F7F7F;
    constexpr Word firstByteHighBit = 0x80;
    constexpr unsigned char asciiLimit = 0x7F;
    constexpr unsigned char lowerCaseBit = 0x20;
}

constexpr Word byteAt(const char* text, std::size_t at) noexcept
{
    return static_cast<unsigned char>(text[at]);
}

/// The `k::wordSize` bytes of `text` from `at`, which must all be inside it.
constexpr Word loadWord(std::string_view text, std::size_t at) noexcept
{
    const char* const bytes = text.data() + at;
    return byteAt(bytes, 0)
        | byteAt(bytes, 1) << k::bitsPerByte
        | byteAt(bytes, 2) << (2 * k::bitsPerByte)
        | byteAt(bytes, 3) << (3 * k::bitsPerByte)
        | byteAt(bytes, 4) << (4 * k::bitsPerByte)
        | byteAt(bytes, 5) << (5 * k::bitsPerByte)
        | byteAt(bytes, 6) << (6 * k::bitsPerByte)
        | byteAt(bytes, 7) << (7 * k::bitsPerByte);
}

constexpr Word broadcast(unsigned char byte) noexcept
{
    return k::onePerByte * byte;
}

/// The high bit of each byte of `word` that differs from `byte`, for a `word`
/// with no byte above 0x7F; a byte above 0x7F may also clear the bit of the
/// byte above it. Bits below the high bits are unspecified.
constexpr Word bytesDifferingFrom(Word word, unsigned char byte) noexcept
{
    return (word ^ broadcast(byte)) + k::lowBitsPerByte;
}

/// The high bit of each byte of `word` that is at least `limit`, for a `word`
/// with no byte above 0x7F and a `limit` of at most 0x80; a byte above 0x7F
/// may also set the bit of the byte above it. Bits below the high bits are
/// unspecified.
constexpr Word bytesAtLeast(Word word, unsigned char limit) noexcept
{
    return word + broadcast(static_cast<unsigned char>(k::asciiLimit + 1 - limit));
}

/// The high bit of each byte of `word` that is above 0x7F.
constexpr Word bytesAboveAscii(Word word) noexcept
{
    return word & k::highBitPerByte;
}

/// The high bit of each byte of `word` that equals `byte`, and no other bit.
constexpr Word bytesEqualTo(Word word, char byte) noexcept
{
    const Word difference = word ^ broadcast(static_cast<unsigned char>(byte));
    return ~(((difference & k::lowBitsPerByte) + k::lowBitsPerByte) | difference) & k::highBitPerByte;
}

/// Whether two adjacent bytes are both marked in `bytes`, a result of
/// `bytesEqualTo`.
constexpr bool hasAdjacentPair(Word bytes) noexcept
{
    return (bytes & (bytes >> k::bitsPerByte)) != 0;
}

/// Whether the first byte is marked in `bytes`, a result of `bytesEqualTo`.
constexpr bool hasFirstByte(Word bytes) noexcept
{
    return (bytes & k::firstByteHighBit) != 0;
}

/// The number of bytes after the last byte marked in `bytes`, a nonzero
/// result of `bytesEqualTo`.
constexpr std::size_t numBytesAfterLast(Word bytes) noexcept
{
    Word upToLast = bytes;
    upToLast |= upToLast >> k::bitsPerByte;
    upToLast |= upToLast >> (2 * k::bitsPerByte);
    upToLast |= upToLast >> (4 * k::bitsPerByte);
    const Word after = ~upToLast & k::highBitPerByte;
    return static_cast<std::size_t>(((after >> (k::bitsPerByte - 1)) * k::onePerByte) >> ((k::wordSize - 1) * k::bitsPerByte));
}

}
