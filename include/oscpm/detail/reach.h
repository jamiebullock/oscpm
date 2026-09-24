/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace oscpm::detail
{

namespace k
{
    constexpr std::size_t bitsPerWord = 64;
    constexpr std::size_t reachWords = kMaxAddressPartLength / bitsPerWord + 1;
    constexpr std::size_t smallReachWords = 1;
}

template <std::size_t NumWords>
class Reach
{
public:
    constexpr explicit Reach(std::size_t partLength) noexcept
        : m_numWords(partLength / k::bitsPerWord + 1)
    {
    }

    constexpr bool test(std::size_t position) const noexcept
    {
        return ((m_words[position / k::bitsPerWord] >> (position % k::bitsPerWord)) & 1U) != 0U;
    }

    constexpr void set(std::size_t position) noexcept
    {
        m_words[position / k::bitsPerWord] |= std::uint64_t { 1 } << (position % k::bitsPerWord);
    }

    constexpr void setFrom(std::size_t first, std::size_t last) noexcept
    {
        for (std::size_t position = first; position <= last; ++position)
        {
            set(position);
        }
    }

    constexpr bool empty() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                return false;
            }
        }
        return true;
    }

    constexpr std::size_t lowest() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                std::size_t position = word * k::bitsPerWord;
                while (!test(position))
                {
                    ++position;
                }
                return position;
            }
        }
        return npos;
    }

private:
    std::array<std::uint64_t, NumWords> m_words { };
    std::size_t m_numWords;
};

}
