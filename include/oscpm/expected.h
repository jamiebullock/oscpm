/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace oscpm
{

namespace detail
{

    template <typename E, bool = std::is_enum_v<E>>
    struct IsScopedEnum : std::false_type
    {
    };

    template <typename E>
    struct IsScopedEnum<E, true> : std::bool_constant<!std::is_convertible_v<E, std::underlying_type_t<E>>>
    {
    };

    /// Kept out of the constexpr callers: on some platforms `assert`
    /// defines a static, which a constexpr function may not contain.
    [[noreturn]] inline void abortWithoutValue() noexcept
    {
        assert(false && "an Expected holding an error has no value");
        std::abort();
    }

    [[noreturn]] inline void abortWithoutError() noexcept
    {
        assert(false && "an Expected holding a value has no error");
        std::abort();
    }

}

/// Either a `T` or the error `E` that prevented one, with the member names of
/// `std::expected`. Reading the side it does not hold asserts in a build
/// without `NDEBUG`, and otherwise calls `std::abort`.
/// @tparam E a scoped enumeration that neither converts to nor from `T`
template <typename T, typename E>
class Expected
{
    static_assert(detail::IsScopedEnum<E>::value, "Expected needs an E that is a scoped enumeration");
    static_assert(!std::is_convertible_v<E, T> && !std::is_convertible_v<T, E>, "Expected needs a T and an E that do not convert to each other");

public:
    constexpr Expected(T value)
        : m_storage(std::in_place_index<kValue>, std::move(value))
    {
    }

    constexpr Expected(E error)
        : m_storage(std::in_place_index<kError>, error)
    {
    }

    /// Whether this holds a `T`.
    constexpr bool has_value() const noexcept
    {
        return m_storage.index() == kValue;
    }

    /// `has_value()`.
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    /// The `T`; requires `has_value()`.
    constexpr const T& operator*() const&
    {
        requireValue();
        return std::get<kValue>(m_storage);
    }

    /// The `T`; requires `has_value()`.
    constexpr T& operator*() &
    {
        requireValue();
        return std::get<kValue>(m_storage);
    }

    /// The `T`, to move from; requires `has_value()`.
    constexpr T&& operator*() &&
    {
        requireValue();
        return std::get<kValue>(std::move(m_storage));
    }

    /// The `T`'s members; requires `has_value()`.
    constexpr const T* operator->() const
    {
        return std::addressof(**this);
    }

    /// The `T`'s members; requires `has_value()`.
    constexpr T* operator->()
    {
        return std::addressof(**this);
    }

    /// The error; requires `!has_value()`.
    constexpr E error() const
    {
        requireError();
        return std::get<kError>(m_storage);
    }

private:
    static constexpr std::size_t kValue = 0;
    static constexpr std::size_t kError = 1;

    constexpr void requireValue() const noexcept
    {
        if (!has_value())
        {
            detail::abortWithoutValue();
        }
    }

    constexpr void requireError() const noexcept
    {
        if (has_value())
        {
            detail::abortWithoutError();
        }
    }

    std::variant<T, E> m_storage;
};

}
