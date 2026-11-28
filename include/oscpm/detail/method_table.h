/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/address_index.h>
#include <oscpm/detail/compiler.h>
#include <oscpm/detail/match.h>
#include <oscpm/detail/prepared.h>
#include <oscpm/detail/syntax.h>
#include <oscpm/error.h>
#include <oscpm/expected.h>
#include <oscpm/pattern.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace oscpm::detail
{

template <typename T, typename MemoTable>
class MethodTable
{
public:
    MethodTable() = default;
    MethodTable(const MethodTable&) = default;
    MethodTable(MethodTable&&) = default;

    MethodTable& operator=(const MethodTable& other)
    {
        return *this = MethodTable(other);
    }

    MethodTable& operator=(MethodTable&&) = default;

    bool insert(std::string_view address, T value)
    {
        const auto position = lowerBound(m_methods, address);
        if (position != m_methods.end() && position->address == address)
        {
            return false;
        }
        m_index.reserveForInsert();
        m_memo.ensureBuckets();
        const auto offset = position - m_methods.begin();
        // Nothing after this call can throw, and the call has no effects when it throws, so a failed insert leaves the table as it was.
        m_methods.insert(position, Method { StoredAddress(address), std::move(value) });
        m_memo.forget();
        m_index.insert(static_cast<std::size_t>(offset), hashOf(address));
        return true;
    }

    bool erase(std::string_view address) noexcept
    {
        const auto position = lowerBound(m_methods, address);
        if (position == m_methods.end() || position->address != address)
        {
            return false;
        }
        const auto offset = position - m_methods.begin();
        m_memo.forget();
        m_methods.erase(position);
        m_index.erase(static_cast<std::size_t>(offset));
        return true;
    }

    T* find(std::string_view address) noexcept
    {
        return findIn(*this, address);
    }

    const T* find(std::string_view address) const noexcept
    {
        return findIn(*this, address);
    }

    template <typename Visitor>
    MatchResult visit(std::string_view text, Visitor& visitor)
    {
        return dispatchIn(*this, text, visitor);
    }

    template <typename Visitor>
    MatchResult visit(std::string_view text, Visitor& visitor) const
    {
        return dispatchIn(*this, text, visitor);
    }

    template <typename Visitor>
    void visit(Visitor& visitor)
    {
        for (Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    template <typename Visitor>
    void visit(Visitor& visitor) const
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
        StoredAddress address;
        T value;
    };

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

    template <typename Self>
    static auto findIn(Self& self, std::string_view address) noexcept
    {
        auto& methods = self.m_methods;
        std::size_t index = 0;
        const bool found = self.m_index.find(hashOf(address), [&](std::size_t candidate)
            { return methods[candidate].address == address; }, index);
        return found ? &methods[index].value : nullptr;
    }

    template <typename Self, typename Visitor>
    static MatchResult dispatchIn(Self& self, std::string_view text, Visitor& visitor)
    {
        std::size_t numVisited = 0;
        if (dispatchKnown(self, text, visitor, numVisited))
        {
            return MatchResult { numVisited, std::nullopt };
        }
        const Expected<Pattern, PatternError> parsed = Pattern::parse(text);
        if (!parsed)
        {
            return MatchResult { 0, parsed.error() };
        }
        return MatchResult { dispatchParsed(self, *parsed, visitor), std::nullopt };
    }

    OSCPM_NOINLINE static bool matchUnprepared(std::string_view pattern, const StoredAddress& address) noexcept
    {
        return matchParts(PatternCursor(pattern), StoredAddressCursor(address));
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
    static std::size_t dispatchParsed(Self& self, const Pattern& pattern, Visitor& visitor)
    {
        auto& methods = self.m_methods;
        const std::string_view text = pattern.text();
        if (pattern.isLiteral())
        {
            // `dispatchKnown` found no address equal to the text, and a literal pattern matches no other.
            if (self.m_memo.accepts(text))
            {
                self.m_memo.rememberNoMatch(text, hashOf(text));
            }
            return 0;
        }

        const bool memoisable = self.m_memo.accepts(text);
        typename MemoTable::Results found;
        std::size_t numFound = 0;
        PreparedParts prepared;
        const std::size_t numPrepared = prepareParts(text, prepared);
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            auto& method = methods[index];
            const bool matched = numPrepared == npos
                ? matchUnprepared(text, method.address)
                : matchParts(PreparedCursor(prepared, numPrepared), StoredAddressCursor(method.address));
            if (matched)
            {
                if constexpr (MemoTable::kInlineResults != 0)
                {
                    if (numFound < MemoTable::kInlineResults)
                    {
                        found[numFound] = static_cast<std::uint32_t>(index);
                    }
                }
                ++numFound;
                visitor(std::string_view(method.address), method.value);
            }
        }

        if (memoisable && numFound <= MemoTable::kInlineResults)
        {
            self.m_memo.store(text, hashOf(text), found, numFound);
        }
        return numFound;
    }

    std::vector<Method> m_methods;
    AddressIndex m_index;
    mutable MemoTable m_memo;
};

}
