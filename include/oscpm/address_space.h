/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/dispatch_memo.h>
#include <oscpm/detail/method_table.h>
#include <oscpm/error.h>
#include <oscpm/pattern.h>

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
    std::optional<ParseError> error;
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

    /// Registers `value` under `address`. Fails with the `validateAddress`
    /// fault, or `Duplicate` when the address is already registered.
    std::optional<Error> add(std::string_view address, T value)
    {
        if (const std::optional<ParseError> fault = validateAddress(address))
        {
            return fault->kind;
        }
        return m_table.insert(address, std::move(value)) ? std::nullopt : std::optional<Error>(Error::Duplicate);
    }

    /// Unregisters `address`. Fails with the `validateAddress` fault, or
    /// `NotFound` when the address is not registered.
    std::optional<Error> remove(std::string_view address)
    {
        if (const std::optional<ParseError> fault = validateAddress(address))
        {
            return fault->kind;
        }
        return m_table.erase(address) ? std::nullopt : std::optional<Error>(Error::NotFound);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches, in bytewise address order, and returns how many.
    /// The visitor may look up or dispatch on this space but must not add or
    /// remove methods.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor)
    {
        return m_table.lookup(pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor) const
    {
        return m_table.lookup(pattern, visitor);
    }

    /// `Pattern::parse` then `lookup`; a malformed pattern visits nothing and
    /// is reported.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        return m_table.template dispatch<DispatchResult>(pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method,
    /// in bytewise address order, under the same visitor rule as `lookup`.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        m_table.forEach(visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    void forEach(Visitor&& visitor) const
    {
        m_table.forEach(visitor);
    }

    /// The number of registered methods.
    std::size_t size() const noexcept
    {
        return m_table.size();
    }

private:
    using Table = detail::MethodTable<T, detail::DispatchMemo<CacheBits, InlineResults, kMaxMemoPatternLength>>;

    Table m_table;
};

}
