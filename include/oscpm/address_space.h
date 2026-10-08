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

/// The longest pattern whose matches are kept; a longer one is matched afresh
/// every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// The outcome of `AddressSpace::visit` and `AddressSpace::dispatch`.
struct MatchResult
{
    std::size_t matched; ///< the number of values visited or called
    std::optional<PatternError> error; ///< the parse fault when the pattern was malformed and no value was reached
};

/// Values of type `T` registered under well-formed addresses, which a pattern
/// selects for `visit` and `dispatch`. When `T` is callable, each value is an
/// OSC method and `dispatch` dispatches to the methods a pattern matches. No
/// member function allocates unless its documentation says so; copying the
/// space allocates. Not safe for concurrent use.
/// @tparam T an object type that is move-constructible and move-assignable;
/// copying the space also needs it to be copyable
/// @tparam Memo whether the matches of a pattern are kept until the next `add`
/// or `remove`
/// @tparam CacheBits the memo has `1 << CacheBits` entries, each holding one
/// pattern of up to `kMaxMemoPatternLength` bytes
/// @tparam InlineResults the most matches a kept result lists; a pattern that
/// matches more, or is too long, is delivered in full but not kept
template <typename T, bool Memo = true, unsigned CacheBits = 8, std::size_t InlineResults = 1024>
class AddressSpace
{
    static_assert(std::is_object_v<T> && std::is_move_constructible_v<T> && std::is_move_assignable_v<T>, "AddressSpace needs a T that is an object type, move-constructible and move-assignable");

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
        assert(m_openVisits.none() && "a visitor must not add to or remove from the space that called it");
        assert(!detail::validateAddress(address.text()) && "the bytes an Address views must stay unchanged after it is parsed");
        return m_table.insert(address.text(), std::move(value));
    }

    /// Unregisters `address`; allocates. Returns false when the address is
    /// not registered, and asserts as `add` does.
    bool remove(const Address& address)
    {
        assert(m_openVisits.none() && "a visitor must not add to or remove from the space that called it");
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
    /// for every registered address it matches, in bytewise address order. A
    /// malformed pattern visits nothing and is returned as the result's
    /// `error`. The visitor may call `visit`, `dispatch` and `find` on this
    /// space and copy it, but must not add to it, remove from it, move from it
    /// or assign to it; a build without `NDEBUG` asserts when it adds or
    /// removes.
    template <typename Visitor>
    MatchResult visit(std::string_view pattern, Visitor&& visitor)
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, T&>, "visit needs a visitor callable as visitor(std::string_view address, T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope scope(m_openVisits);
        return m_table.template visit<MatchResult>(pattern, visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    MatchResult visit(std::string_view pattern, Visitor&& visitor) const
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, const T&>, "visit needs a visitor callable as visitor(std::string_view address, const T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope scope(m_openVisits);
        return m_table.template visit<MatchResult>(pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every
    /// registered address, in bytewise address order, under the same visitor
    /// rule as the pattern overload.
    template <typename Visitor>
    void visit(Visitor&& visitor)
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, T&>, "visit needs a visitor callable as visitor(std::string_view address, T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope scope(m_openVisits);
        m_table.visit(visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    void visit(Visitor&& visitor) const
    {
        static_assert(std::is_invocable_v<Visitor&, std::string_view, const T&>, "visit needs a visitor callable as visitor(std::string_view address, const T& value)");
        [[maybe_unused]] const typename OpenVisits::Scope scope(m_openVisits);
        m_table.visit(visitor);
    }

    /// Parses `pattern` and calls `std::invoke(value, args...)` on the value
    /// of every registered address it matches, in bytewise address order,
    /// under the same visitor rule as `visit`. A value that is a pointer to a
    /// member takes its object as the first of `args`. A malformed pattern
    /// calls nothing and is returned as the result's `error`. Every call
    /// receives the same `args` objects; `dispatch` itself does not move from
    /// them.
    template <typename... Args>
    MatchResult dispatch(std::string_view pattern, Args&&... args)
    {
        static_assert(std::is_invocable_v<T&, Args&...>, "dispatch needs a T that is callable with these arguments; use visit for other values");
        return visit(pattern, [&](std::string_view, T& value)
            { static_cast<void>(std::invoke(value, args...)); });
    }

    /// The `const` overload; calls `std::invoke(value, args...)` on a `const T`.
    template <typename... Args>
    MatchResult dispatch(std::string_view pattern, Args&&... args) const
    {
        static_assert(std::is_invocable_v<const T&, Args&...>, "dispatch needs a T that is callable with these arguments; use visit for other values");
        return visit(pattern, [&](std::string_view, const T& value)
            { static_cast<void>(std::invoke(value, args...)); });
    }

    /// The number of registered addresses.
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
