/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/address.h>
#include <oscpm/detail/dispatch_memo.h>
#include <oscpm/detail/method_table.h>
#include <oscpm/detail/validate.h>
#include <oscpm/error.h>
#include <oscpm/pattern.h>

#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace oscpm
{

/// The longest pattern whose dispatch result is kept; a longer one is matched
/// afresh every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// The outcome of `AddressSpace::dispatch` and `AddressSpace::invoke`.
struct DispatchResult
{
    std::size_t matched; ///< the number of methods visited or called
    std::optional<Error> error; ///< the parse fault when the pattern was malformed and no method was reached
};

/// A set of methods, each a well-formed address with a value of type `T`,
/// that a pattern is dispatched to. No method allocates unless its
/// documentation says so; copying the space allocates. Not safe for
/// concurrent use.
/// @tparam Memo whether a dispatch's result is kept until the next `add` or
/// `remove`
/// @tparam CacheBits the memo has `1 << CacheBits` entries, each holding one
/// pattern of up to `kMaxMemoPatternLength` bytes
/// @tparam InlineResults the most methods a kept result lists; a dispatch
/// that matches more, or whose pattern is too long, is delivered in full but
/// not kept
template <typename T, bool Memo = true, unsigned CacheBits = 8, std::size_t InlineResults = 1024>
class AddressSpace
{
public:
    AddressSpace()
        : m_table(Memo)
    {
    }

    /// Registers `value` under `address`; allocates. Returns false, and
    /// registers nothing, when the address is already registered. A build
    /// without `NDEBUG` asserts when the bytes the address views no longer
    /// parse as an address.
    bool add(const Address& address, T value)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        assert(!detail::validateAddress(address.text()) && "the bytes an Address views must stay unchanged after it is parsed");
        return m_table.insert(address.text(), std::move(value));
    }

    /// Unregisters `address`; allocates. Returns false when the address is
    /// not registered, and asserts as `add` does.
    bool remove(const Address& address)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        assert(!detail::validateAddress(address.text()) && "the bytes an Address views must stay unchanged after it is parsed");
        return m_table.erase(address.text());
    }

    /// The value registered under `address`, or null when there is none. The
    /// pointer is valid until the next `add` or `remove`.
    T* find(std::string_view address) noexcept
    {
        return m_table.find(address);
    }

    /// The `const` overload; returns `const T*`.
    const T* find(std::string_view address) const noexcept
    {
        return m_table.find(address);
    }

    /// Parses `pattern` and calls `visitor(std::string_view address, T& value)`
    /// for every method it matches, in bytewise address order. A malformed
    /// pattern visits nothing and is returned as the result's `error`. The
    /// visitor may dispatch on this space, call `find` and copy the space, but
    /// must not add or remove methods, move from it or assign to it; a build
    /// without `NDEBUG` asserts when it adds or removes.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, T&>, "dispatch needs a visitor callable as visitor(std::string_view address, T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, const T&>, "dispatch needs a visitor callable as visitor(std::string_view address, const T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// Parses `pattern` and calls `std::invoke(value, args...)` on every method
    /// it matches, in bytewise address order, under the same visitor rule as
    /// `dispatch`. A value that is a pointer to a member takes its object as
    /// the first of `args`.
    /// A malformed pattern calls nothing and is returned as the result's
    /// `error`. Every call receives the same `args` objects; `invoke` itself
    /// does not move from them.
    template <typename... Args>
    DispatchResult invoke(std::string_view pattern, Args&&... args)
    {
        static_assert(std::is_invocable_v<T&, Args&...>, "invoke needs a T that is callable with these arguments; use dispatch for other values");
        return dispatch(pattern, [&](std::string_view, T& value)
            { static_cast<void>(std::invoke(value, args...)); });
    }

    /// The `const` overload; calls `std::invoke(value, args...)` on a `const T`.
    template <typename... Args>
    DispatchResult invoke(std::string_view pattern, Args&&... args) const
    {
        static_assert(std::is_invocable_v<const T&, Args&...>, "invoke needs a T that is callable with these arguments; use dispatch for other values");
        return dispatch(pattern, [&](std::string_view, const T& value)
            { static_cast<void>(std::invoke(value, args...)); });
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method,
    /// in bytewise address order, under the same visitor rule as `dispatch`.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, T&>, "forEach needs a visitor callable as visitor(std::string_view address, T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        m_table.forEach(visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    void forEach(Visitor&& visitor) const
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, const T&>, "forEach needs a visitor callable as visitor(std::string_view address, const T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        m_table.forEach(visitor);
    }

    /// The number of registered methods.
    std::size_t size() const noexcept
    {
        return m_table.size();
    }

private:
    using Table = detail::MethodTable<T, detail::DispatchMemo<CacheBits, Memo ? InlineResults : 0, kMaxMemoPatternLength>>;

    class OpenVisits
    {
    public:
        OpenVisits() = default;

        OpenVisits(const OpenVisits&) noexcept
        {
        }

        OpenVisits& operator=(const OpenVisits&) noexcept
        {
            return *this;
        }

        bool none() const noexcept
        {
            return m_count == 0;
        }

        class Scope
        {
        public:
#ifdef NDEBUG
            explicit Scope(OpenVisits&) noexcept
            {
            }
#else
            explicit Scope(OpenVisits& visits) noexcept
                : m_visits(visits)
            {
                ++m_visits.m_count;
            }

            ~Scope()
            {
                --m_visits.m_count;
            }

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            OpenVisits& m_visits;
#endif
        };

    private:
        std::size_t m_count = 0;
    };

    Table m_table;
    mutable OpenVisits m_openVisits;
};

}
