/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/address_index.h>
#include <oscpm/detail/dispatch_memo.h>
#include <oscpm/detail/match.h>
#include <oscpm/detail/prepared.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>
#include <oscpm/pattern.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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
        : m_memo(Memo)
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
        const auto position = lowerBound(m_methods, address);
        if (position != m_methods.end() && position->address == address)
        {
            return Error::Duplicate;
        }
        std::vector<std::size_t> partEnds;
        if constexpr (kMethodMovesCannotThrow)
        {
            partEnds = detail::partEndsOf(address);
            if (m_partEnds.size() == m_partEnds.capacity())
            {
                m_partEnds.reserve(2 * m_partEnds.size() + 1);
            }
            m_index.reserveForInsert();
        }
        const auto offset = position - m_methods.begin();
        m_methods.insert(position, Method { std::string(address), std::move(value) });
        m_memo.forget();
        if constexpr (kMethodMovesCannotThrow)
        {
            m_partEnds.insert(m_partEnds.begin() + offset, std::move(partEnds));
            m_index.insert(static_cast<std::size_t>(offset), detail::hashOf(address));
        }
        return std::nullopt;
    }

    /// Unregisters `address`. Fails with the `validateAddress` fault, or
    /// `NotFound` when the address is not registered.
    std::optional<Error> remove(std::string_view address)
    {
        if (const std::optional<ParseError> fault = validateAddress(address))
        {
            return fault->kind;
        }
        const auto position = lowerBound(m_methods, address);
        if (position == m_methods.end() || position->address != address)
        {
            return Error::NotFound;
        }
        const auto offset = position - m_methods.begin();
        m_methods.erase(position);
        m_memo.forget();
        if constexpr (kMethodMovesCannotThrow)
        {
            m_partEnds.erase(m_partEnds.begin() + offset);
            m_index.erase(static_cast<std::size_t>(offset));
        }
        return std::nullopt;
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches, in bytewise address order, and returns how many.
    /// The visitor may look up or dispatch on this space but must not add or
    /// remove methods.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor)
    {
        return lookupIn(*this, pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor) const
    {
        return lookupIn(*this, pattern, visitor);
    }

    /// `Pattern::parse` then `lookup`; a malformed pattern visits nothing and
    /// is reported.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        return dispatchIn(*this, pattern, visitor);
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        return dispatchIn(*this, pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method,
    /// in bytewise address order, under the same visitor rule as `lookup`.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        for (Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    /// As above, with `const T&`.
    template <typename Visitor>
    void forEach(Visitor&& visitor) const
    {
        for (const Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    /// The number of registered methods.
    std::size_t size() const noexcept
    {
        return m_methods.size();
    }

private:
    using MemoTable = detail::DispatchMemo<CacheBits, InlineResults, kMaxMemoPatternLength>;

    struct Method
    {
        std::string address;
        T value;
    };

    static constexpr bool kMethodMovesCannotThrow = std::is_nothrow_move_constructible_v<Method> && std::is_nothrow_move_assignable_v<Method>;

    template <typename Methods>
    static auto lowerBound(Methods& methods, std::string_view address)
    {
        return std::lower_bound(methods.begin(), methods.end(), address, [](const Method& method, std::string_view candidate)
            { return method.address < candidate; });
    }

    template <typename Methods, typename Visitor>
    static void visitMethod(Methods& methods, std::size_t index, Visitor& visitor)
    {
        auto& method = methods[index];
        visitor(std::string_view(method.address), method.value);
    }

    template <typename Self, typename Visitor>
    static DispatchResult dispatchIn(Self& self, std::string_view pattern, Visitor& visitor)
    {
        std::size_t numVisited = 0;
        if (dispatchKnown(self, pattern, visitor, numVisited))
        {
            return DispatchResult { numVisited, std::nullopt };
        }
        const ParseResult parsed = Pattern::parse(pattern);
        if (!parsed)
        {
            return DispatchResult { 0, parsed.error() };
        }
        return DispatchResult { lookupIn(self, parsed.pattern(), visitor), std::nullopt };
    }

    template <typename Self, typename Visitor>
    static bool dispatchKnown(Self& self, std::string_view text, Visitor& visitor, std::size_t& numVisited)
    {
        auto& methods = self.m_methods;
        const std::size_t hash = detail::hashOf(text);
        if (self.m_memo.accepts(text) && self.m_memo.visit(text, hash, methods, visitor, numVisited))
        {
            return true;
        }
        std::size_t index = 0;
        if (!self.m_index.find(hash, [&](std::size_t candidate)
                { return methods[candidate].address == text; }, index))
        {
            return false;
        }
        visitMethod(methods, index, visitor);
        numVisited = 1;
        return true;
    }

    template <typename Self, typename Visitor>
    static std::size_t lookupIn(Self& self, const Pattern& pattern, Visitor& visitor)
    {
        auto& methods = self.m_methods;
        const std::string_view text = pattern.text();
        if (pattern.isLiteral())
        {
            const auto position = lowerBound(methods, text);
            if (position == methods.end() || position->address != text)
            {
                if (self.m_memo.accepts(text) && methods.size() <= MemoTable::kMaxMethods)
                {
                    self.m_memo.rememberNoMatch(text, detail::hashOf(text));
                }
                return 0;
            }
            visitor(std::string_view(position->address), position->value);
            return 1;
        }

        const bool memoisable = self.m_memo.accepts(text) && methods.size() <= MemoTable::kMaxMethods;
        const std::size_t hash = memoisable ? detail::hashOf(text) : 0;
        if (memoisable)
        {
            std::size_t numMemoised = 0;
            if (self.m_memo.visit(text, hash, methods, visitor, numMemoised))
            {
                return numMemoised;
            }
        }

        typename MemoTable::Results found { };
        std::size_t numFound = 0;
        detail::PreparedParts prepared;
        const std::size_t numPrepared = kMethodMovesCannotThrow ? detail::prepareParts(text, prepared) : detail::npos;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            auto& method = methods[index];
            const bool matched = numPrepared == detail::npos
                ? pattern.matches(method.address)
                : detail::matchParts(detail::PreparedCursor(prepared, numPrepared), detail::StoredAddressCursor(method.address, self.m_partEnds[index]));
            if (matched)
            {
                if (numFound < InlineResults)
                {
                    found[numFound] = static_cast<std::uint32_t>(index);
                }
                ++numFound;
                visitor(std::string_view(method.address), method.value);
            }
        }

        if (memoisable && numFound <= InlineResults)
        {
            self.m_memo.store(text, hash, found, numFound);
        }
        return numFound;
    }

    std::vector<Method> m_methods;
    std::vector<std::vector<std::size_t>> m_partEnds;
    detail::AddressIndex m_index;
    mutable MemoTable m_memo;
};

}
