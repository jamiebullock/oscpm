/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/oscpm.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oscpm
{

/// The longest pattern a lookup memoises; a longer one is matched afresh
/// every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// A set of OSC methods, each a well-formed address with a value of type
/// `T`, that a pattern is dispatched to. Addresses are kept in bytewise
/// order. A literal pattern is found by binary search; any other is
/// matched against every method, with the result memoised until the next
/// `add` or `remove` when `Memo` is true. The memo has `1 << CacheBits`
/// entries and keeps a result of at most `InlineResults` methods for a
/// pattern of at most `kMaxMemoPatternLength` bytes. `add` and `remove`
/// allocate; `lookup` and `forEach` never do. Not safe for concurrent use.
template <typename T, bool Memo = true, unsigned CacheBits = 8, std::size_t InlineResults = 64>
class AddressSpace
{
public:
    AddressSpace()
        : m_memo(Memo ? kNumBuckets : 0)
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
        const auto position = lowerBound(address);
        if (position != m_methods.end() && position->address == address)
        {
            return Error::Duplicate;
        }
        m_methods.insert(position, Method { std::string(address), std::move(value) });
        ++m_generation;
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
        const auto position = lowerBound(address);
        if (position == m_methods.end() || position->address != address)
        {
            return Error::NotFound;
        }
        m_methods.erase(position);
        ++m_generation;
        return std::nullopt;
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches, in bytewise address order, and returns how many.
    /// The visitor must not add or remove methods.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor)
    {
        return lookupIn(m_methods, pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, const T& value)` for every
    /// method `pattern` matches, in bytewise address order, and returns how
    /// many.
    template <typename Visitor>
    std::size_t lookup(const Pattern& pattern, Visitor&& visitor) const
    {
        return lookupIn(m_methods, pattern, visitor);
    }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// in bytewise address order. The visitor must not add or remove
    /// methods.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        for (Method& method : m_methods)
        {
            visitor(std::string_view(method.address), method.value);
        }
    }

    /// Calls `visitor(std::string_view address, const T& value)` for every
    /// method in bytewise address order.
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
    static constexpr std::size_t kNumBuckets = std::size_t { 1 } << CacheBits;
    static constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
    static constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

    struct Method
    {
        std::string address;
        T value;
    };

    struct Bucket
    {
        std::uint64_t generation = 0;
        std::size_t patternLength = 0;
        std::size_t numResults = 0;
        std::array<char, kMaxMemoPatternLength> pattern { };
        std::array<std::size_t, InlineResults> results { };
    };

    static std::size_t bucketIndex(std::string_view pattern) noexcept
    {
        std::uint64_t hash = kFnvOffset;
        for (const char byte : pattern)
        {
            hash ^= static_cast<unsigned char>(byte);
            hash *= kFnvPrime;
        }
        return static_cast<std::size_t>(hash & (kNumBuckets - 1));
    }

    static bool holds(const Bucket& bucket, std::string_view pattern) noexcept
    {
        return bucket.patternLength == pattern.size() && std::string_view(bucket.pattern.data(), bucket.patternLength) == pattern;
    }

    typename std::vector<Method>::iterator lowerBound(std::string_view address)
    {
        return std::lower_bound(m_methods.begin(), m_methods.end(), address, [](const Method& method, std::string_view candidate)
            { return method.address < candidate; });
    }

    template <typename Methods, typename Visitor>
    std::size_t lookupIn(Methods& methods, const Pattern& pattern, Visitor& visitor) const
    {
        const std::string_view text = pattern.text();
        if (pattern.isLiteral())
        {
            const auto position = std::lower_bound(methods.begin(), methods.end(), text, [](const Method& method, std::string_view candidate)
                { return method.address < candidate; });
            if (position == methods.end() || position->address != text)
            {
                return 0;
            }
            visitor(std::string_view(position->address), position->value);
            return 1;
        }

        const bool memoisable = Memo && text.size() <= kMaxMemoPatternLength;
        Bucket* bucket = memoisable ? &m_memo[bucketIndex(text)] : nullptr;
        if (bucket != nullptr && bucket->generation == m_generation && holds(*bucket, text))
        {
            for (std::size_t i = 0; i < bucket->numResults; ++i)
            {
                auto& method = methods[bucket->results[i]];
                visitor(std::string_view(method.address), method.value);
            }
            return bucket->numResults;
        }

        std::array<std::size_t, InlineResults> found { };
        std::size_t numFound = 0;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            auto& method = methods[index];
            if (pattern.matches(method.address))
            {
                if (numFound < InlineResults)
                {
                    found[numFound] = index;
                }
                ++numFound;
                visitor(std::string_view(method.address), method.value);
            }
        }

        if (bucket != nullptr && numFound <= InlineResults)
        {
            bucket->generation = m_generation;
            bucket->patternLength = text.size();
            bucket->numResults = numFound;
            std::copy(text.begin(), text.end(), bucket->pattern.begin());
            bucket->results = found;
        }
        return numFound;
    }

    std::vector<Method> m_methods;
    mutable std::vector<Bucket> m_memo;
    std::uint64_t m_generation = 1;
};

}
