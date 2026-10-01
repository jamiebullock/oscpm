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

/// The longest pattern a lookup memoises; a longer one is matched afresh
/// every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// The number of methods `dispatch` visited, and the parse fault when the
/// pattern was malformed and nothing was visited.
struct DispatchResult
{
    std::size_t matched;
    std::optional<Error> error;
};

/// A set of methods, each a well-formed address with a value of type `T`,
/// that a pattern is dispatched to. `add` and `remove` allocate; nothing
/// else does. Not safe for concurrent use.
/// @tparam Memo whether a lookup's result is kept until the next `add` or `remove`
/// @tparam CacheBits the memo holds `1 << CacheBits` patterns of up to `kMaxMemoPatternLength` bytes
/// @tparam InlineResults the most methods a memoised result holds; a lookup beyond either limit is delivered in full but not kept
template <typename T, bool Memo = true, unsigned CacheBits = 8, std::size_t InlineResults = 1024>
class AddressSpace
{
public:
    AddressSpace()
        : m_table(Memo)
    {
    }

    /// Registers `value` under `address`. Fails with the first fault in the
    /// address, or `Duplicate` when the address is already registered.
    std::optional<Error> add(std::string_view address, T value)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        if (const std::optional<Error> fault = detail::validateAddress(address))
        {
            return fault;
        }
        return m_table.insert(address, std::move(value)) ? std::nullopt : std::optional<Error>(Error::Duplicate);
    }

    /// Unregisters `address`. Fails with the first fault in the address, or
    /// `NotFound` when the address is not registered.
    std::optional<Error> remove(std::string_view address)
    {
        assert(m_openVisits.none() && "a visitor must not add or remove methods on the space that called it");
        if (const std::optional<Error> fault = detail::validateAddress(address))
        {
            return fault;
        }
        return m_table.erase(address) ? std::nullopt : std::optional<Error>(Error::NotFound);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches, in bytewise address order, and returns how many.
    /// The visitor may look up or dispatch on this space and may copy it, but
    /// must not add or remove methods, move from it or assign to it; a build
    /// without `NDEBUG` asserts when it adds or removes.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor)
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.lookup(pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor) const
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.lookup(pattern, visitor);
    }

    /// `Pattern::parse` then `lookup`; a malformed pattern visits nothing and
    /// is reported.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method,
    /// in bytewise address order, under the same visitor rule as `lookup`.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        [[maybe_unused]] const typename OpenVisits::Scope visit(m_openVisits);
        m_table.forEach(visitor);
    }

    /// As above, with `const T&`.
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
