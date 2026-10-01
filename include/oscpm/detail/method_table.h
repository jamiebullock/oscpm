/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/address_index.h>
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

namespace oscpm::detail
{

template <typename T, typename MemoTable>
class MethodTable
{
public:
    explicit MethodTable(bool memoised)
        : m_memo(memoised)
    {
    }

    bool insert(std::string_view address, T value)
    {
        const auto position = lowerBound(m_methods, address);
        if (position != m_methods.end() && position->address == address)
        {
            return false;
        }
        std::vector<std::size_t> partEnds;
        if constexpr (kMethodMovesCannotThrow)
        {
            partEnds = partEndsOf(address);
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
            m_index.insert(static_cast<std::size_t>(offset), hashOf(address));
        }
        return true;
    }

    bool erase(std::string_view address)
    {
        const auto position = lowerBound(m_methods, address);
        if (position == m_methods.end() || position->address != address)
        {
            return false;
        }
        const auto offset = position - m_methods.begin();
        m_methods.erase(position);
        m_memo.forget();
        if constexpr (kMethodMovesCannotThrow)
        {
            m_partEnds.erase(m_partEnds.begin() + offset);
            m_index.erase(static_cast<std::size_t>(offset));
        }
        return true;
    }

    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor& visitor)
    {
        return lookupIn(*this, pattern, visitor);
    }

    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor& visitor) const
    {
        return lookupIn(*this, pattern, visitor);
    }

    template <typename Result, typename Visitor>
    Result dispatch(std::string_view text, Visitor& visitor)
    {
        return dispatchIn<Result>(*this, text, visitor);
    }

    template <typename Result, typename Visitor>
    Result dispatch(std::string_view text, Visitor& visitor) const
    {
        return dispatchIn<Result>(*this, text, visitor);
    }

    template <typename Visitor>
    void forEach(Visitor& visitor)
    {
        for (Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    template <typename Visitor>
    void forEach(Visitor& visitor) const
    {
        for (const Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    std::size_t size() const noexcept
    {
        return m_methods.size();
    }

private:
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

    template <typename Result, typename Self, typename Visitor>
    static Result dispatchIn(Self& self, std::string_view text, Visitor& visitor)
    {
        std::size_t numVisited = 0;
        if (dispatchKnown(self, text, visitor, numVisited))
        {
            return Result { numVisited, std::nullopt };
        }
        const ParseResult parsed = Pattern::parse(text);
        if (!parsed)
        {
            return Result { 0, parsed.error() };
        }
        return Result { lookupIn(self, parsed.pattern(), visitor), std::nullopt };
    }

    template <typename Self, typename Visitor>
    static bool dispatchKnown(Self& self, std::string_view text, Visitor& visitor, std::size_t& numVisited)
    {
        auto& methods = self.m_methods;
        const std::size_t hash = hashOf(text);
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
                    self.m_memo.rememberNoMatch(text, hashOf(text));
                }
                return 0;
            }
            visitor(std::string_view(position->address), position->value);
            return 1;
        }

        const bool memoisable = self.m_memo.accepts(text) && methods.size() <= MemoTable::kMaxMethods;
        const std::size_t hash = memoisable ? hashOf(text) : 0;
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
        PreparedParts prepared;
        const std::size_t numPrepared = kMethodMovesCannotThrow ? prepareParts(text, prepared) : npos;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            auto& method = methods[index];
            const bool matched = numPrepared == npos
                ? pattern.matches(method.address)
                : matchParts(PreparedCursor(prepared, numPrepared), StoredAddressCursor(method.address, self.m_partEnds[index]));
            if (matched)
            {
                if (numFound < MemoTable::kInlineResults)
                {
                    found[numFound] = static_cast<std::uint32_t>(index);
                }
                ++numFound;
                visitor(std::string_view(method.address), method.value);
            }
        }

        if (memoisable && numFound <= MemoTable::kInlineResults)
        {
            self.m_memo.store(text, hash, found, numFound);
        }
        return numFound;
    }

    std::vector<Method> m_methods;
    std::vector<std::vector<std::size_t>> m_partEnds;
    AddressIndex m_index;
    mutable MemoTable m_memo;
};

}
