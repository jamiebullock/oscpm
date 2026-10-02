/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/dispatch_memo.h>
#include <oscpm/detail/method_table.h>
#include <oscpm/detail/validate.h>
#include <oscpm/error.h>
#include <oscpm/pattern.h>

#include <cassert>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace oscpm
{

/// The longest pattern whose dispatch result is kept; a longer one is matched
/// afresh every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// The outcome of `AddressSpace::dispatch`.
struct DispatchResult
{
    std::size_t matched; ///< the number of methods visited
    std::optional<Error> error; ///< the parse fault when the pattern was malformed and nothing was visited
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

    /// Registers `value` under `address`; allocates. Fails with the first
    /// fault in the address, or `Duplicate` when the address is already
    /// registered.
    std::optional<Error> add(std::string_view address, T value)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        if (const std::optional<Error> fault = detail::validateAddress(address))
        {
            return fault;
        }
        return m_table.insert(address, std::move(value)) ? std::nullopt : std::optional<Error>(Error::Duplicate);
    }

    /// Unregisters `address`; allocates. Fails with the first fault in the
    /// address, or `NotFound` when the address is not registered.
    std::optional<Error> remove(std::string_view address)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        if (const std::optional<Error> fault = detail::validateAddress(address))
        {
            return fault;
        }
        return m_table.erase(address) ? std::nullopt : std::optional<Error>(Error::NotFound);
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
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// Parses `pattern` and calls `value(args...)` on every method it matches,
    /// in bytewise address order, under the same visitor rule as `dispatch`.
    /// A malformed pattern calls nothing and is returned as the result's
    /// `error`. Every call receives the same `args` objects, so none is moved
    /// from.
    template <typename... Args>
    DispatchResult invoke(std::string_view pattern, Args&&... args)
    {
        return dispatch(pattern, [&](std::string_view, T& value)
            { value(args...); });
    }

    /// The `const` overload; calls `value(args...)` on a `const T`.
    template <typename... Args>
    DispatchResult invoke(std::string_view pattern, Args&&... args) const
    {
        return dispatch(pattern, [&](std::string_view, const T& value)
            { value(args...); });
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method,
    /// in bytewise address order, under the same visitor rule as `dispatch`.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        m_table.forEach(visitor);
    }

    /// The `const` overload; the visitor receives `const T&`.
    template <typename Visitor>
    void forEach(Visitor&& visitor) const
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        m_table.forEach(visitor);
    }

    /// The number of registered methods.
    std::size_t size() const noexcept
    {
        return m_table.size();
    }

private:
    using Table = detail::MethodTable<T, detail::DispatchMemo<CacheBits, InlineResults, kMaxMemoPatternLength>>;

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
