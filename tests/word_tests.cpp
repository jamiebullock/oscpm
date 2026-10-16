/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/detail/validate.h>
#include <oscpm/detail/word.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

using oscpm::detail::Word;

namespace
{

constexpr std::size_t kWordSize = oscpm::detail::k::wordSize;
constexpr unsigned kBitsPerByte = oscpm::detail::k::bitsPerByte;
constexpr unsigned kNumByteValues = 256;
constexpr unsigned kNumMarkings = 1U << kWordSize;
constexpr Word kHighBit = 0x80;

constexpr unsigned kAbsentOffset = kWordSize + 1;

/// A word holding `value` at `position` and, elsewhere, the bytes that follow
/// `value` in order, none of which equals it or `value + kAbsentOffset`.
std::string wordWith(unsigned value, std::size_t position)
{
    std::string text(kWordSize, '\0');
    for (std::size_t i = 0; i < kWordSize; ++i)
    {
        text[i] = static_cast<char>((value + 1 + i) % kNumByteValues);
    }
    text[position] = static_cast<char>(value);
    return text;
}

/// A word of letters with `value` at `position`.
std::string lettersWith(unsigned value, std::size_t position)
{
    std::string text(kWordSize, 'a');
    text[position] = static_cast<char>(value);
    return text;
}

/// The high bit of each byte whose bit is set in `marking`.
constexpr Word marks(unsigned marking)
{
    Word word = 0;
    for (std::size_t i = 0; i < kWordSize; ++i)
    {
        if (marking & (1U << i))
        {
            word |= kHighBit << (kBitsPerByte * i);
        }
    }
    return word;
}

}

TEST_CASE("loadWord takes the first byte of the text as the low byte of the word")
{
    static_assert(oscpm::detail::loadWord("\x01\x02\x03\x04\x05\x06\x07\x08", 0) == 0x0807060504030201);
    static_assert(oscpm::detail::loadWord("x\x01\x02\x03\x04\x05\x06\x07\x08", 1) == 0x0807060504030201);
}

TEST_CASE("bytesEqualTo marks the equal bytes and no other")
{
    for (std::size_t position = 0; position < kWordSize; ++position)
    {
        for (unsigned value = 0; value < kNumByteValues; ++value)
        {
            INFO("value " << value << " at " << position);
            const Word word = oscpm::detail::loadWord(wordWith(value, position), 0);
            const auto byte = static_cast<char>(value);
            CHECK(oscpm::detail::bytesEqualTo(word, byte) == (kHighBit << (kBitsPerByte * position)));
            CHECK(oscpm::detail::bytesEqualTo(word, static_cast<char>(value + kAbsentOffset)) == 0);
        }
    }
}

TEST_CASE("the ASCII byte tests mark every byte of an ASCII word correctly")
{
    constexpr Word highBits = oscpm::detail::k::highBitPerByte;
    for (std::size_t position = 0; position < kWordSize; ++position)
    {
        const Word here = kHighBit << (kBitsPerByte * position);
        for (unsigned value = 0; value <= 0x7F; ++value)
        {
            INFO("value " << value << " at " << position);
            const Word word = oscpm::detail::loadWord(lettersWith(value, position), 0);
            const auto byte = static_cast<unsigned char>(value);
            CHECK((oscpm::detail::bytesDifferingFrom(word, byte) & highBits) == (value == 'a' ? 0 : highBits & ~here));
            CHECK((oscpm::detail::bytesDifferingFrom(word, 'a') & highBits) == (value == 'a' ? 0 : here));
            CHECK((oscpm::detail::bytesAtLeast(word, 0x21) & highBits) == (value >= 0x21 ? highBits : highBits & ~here));
            CHECK((oscpm::detail::bytesAtLeast(word, 0x80) & highBits) == 0);
            CHECK(oscpm::detail::bytesAboveAscii(word) == 0);
        }
        for (unsigned value = 0x80; value < kNumByteValues; ++value)
        {
            INFO("value " << value << " at " << position);
            const Word word = oscpm::detail::loadWord(lettersWith(value, position), 0);
            CHECK(oscpm::detail::bytesAboveAscii(word) == here);
        }
    }
}

TEST_CASE("the word tests of the validators judge every byte value at every position")
{
    constexpr std::string_view reserved = "#*,?[]{}";
    for (std::size_t position = 0; position < kWordSize; ++position)
    {
        for (unsigned value = 0; value < kNumByteValues; ++value)
        {
            INFO("value " << value << " at " << position);
            const Word word = oscpm::detail::loadWord(lettersWith(value, position), 0);
            const bool printable = value >= 0x21 && value <= 0x7E;
            const bool inPart = printable && reserved.find(static_cast<char>(value)) == std::string_view::npos;
            CHECK(oscpm::detail::hasIllegalAddressByte(word) == !inPart);
            const bool opener = value == '*' || value == '?' || value == '[' || value == '{';
            CHECK(oscpm::detail::hasPatternOpenerOrIllegalByte(word) == (value == 0 || value > 0x7F || opener));
        }
    }
}

TEST_CASE("the marking queries agree with the bits of the marking")
{
    for (unsigned marking = 0; marking < kNumMarkings; ++marking)
    {
        INFO("marking " << marking);
        const Word marked = marks(marking);
        CHECK(oscpm::detail::hasAdjacentPair(marked) == ((marking & (marking >> 1)) != 0));
        CHECK(oscpm::detail::hasFirstByte(marked) == ((marking & 1U) != 0));
        if (marking != 0)
        {
            std::size_t last = 0;
            for (std::size_t i = 0; i < kWordSize; ++i)
            {
                last = marking & (1U << i) ? i : last;
            }
            CHECK(oscpm::detail::numBytesAfterLast(marked) == kWordSize - 1 - last);
        }
    }
}
