/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <string_view>
#include <type_traits>

// Use the custom StringView class as a fallback for string_view when
// explicitly requested or the compiler doesn't fully support std::string_view
#if defined(OSCPM_STRING_VIEW_FALLBACK) || (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE < 8)

namespace oscpm::detail
{

/// A stand-in for the subset of `std::string_view` used by the library
/// adding compatibility on platforms that have an incomplete implementation
class StringView
{
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    constexpr StringView() noexcept = default;

    constexpr StringView(const char* data, std::size_t size) noexcept
        : m_data(data)
        , m_size(size)
    {
    }

    constexpr StringView(const char* text) noexcept
        : StringView(text, lengthOf(text))
    {
    }

    constexpr StringView(std::string_view text) noexcept
        : StringView(text.data(), text.size())
    {
    }

    template <typename Text, typename = std::enable_if_t<std::is_convertible_v<const Text&, std::string_view>>>
    constexpr StringView(const Text& text) noexcept
        : StringView(std::string_view(text))
    {
    }

    constexpr explicit operator std::string_view() const noexcept
    {
        return std::string_view(m_data, m_size);
    }

    constexpr const char* data() const noexcept
    {
        return m_data;
    }

    constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

    constexpr const char* begin() const noexcept
    {
        return m_data;
    }

    constexpr const char* end() const noexcept
    {
        return m_data + m_size;
    }

    constexpr char operator[](std::size_t index) const noexcept
    {
        return m_data[index];
    }

    constexpr StringView substr(std::size_t start, std::size_t count = npos) const noexcept
    {
        const std::size_t from = start < m_size ? start : m_size;
        const std::size_t available = m_size - from;
        return StringView(m_data + from, count < available ? count : available);
    }

    constexpr std::size_t find(char byte, std::size_t from = 0) const noexcept
    {
        for (std::size_t i = from; i < m_size; ++i)
        {
            if (m_data[i] == byte)
            {
                return i;
            }
        }
        return npos;
    }

    constexpr std::size_t find(StringView needle, std::size_t from = 0) const noexcept
    {
        for (std::size_t i = from; needle.size() <= m_size && i <= m_size - needle.size(); ++i)
        {
            if (substr(i, needle.size()) == needle)
            {
                return i;
            }
        }
        return npos;
    }

    friend constexpr bool operator==(StringView left, StringView right) noexcept
    {
        if (left.m_size != right.m_size)
        {
            return false;
        }
        for (std::size_t i = 0; i < left.m_size; ++i)
        {
            if (left.m_data[i] != right.m_data[i])
            {
                return false;
            }
        }
        return true;
    }

    friend constexpr bool operator!=(StringView left, StringView right) noexcept
    {
        return !(left == right);
    }

private:
    static constexpr std::size_t lengthOf(const char* text) noexcept
    {
        std::size_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }
        return length;
    }

    const char* m_data = nullptr;
    std::size_t m_size = 0;
};

}

#else

namespace oscpm::detail
{

using StringView = std::string_view;

}

#endif
