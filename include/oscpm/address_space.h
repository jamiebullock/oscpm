/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/error.h>
#include <oscpm/pattern.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace oscpm::detail
{

namespace k
{
    constexpr std::size_t maxPreparedParts = 64;
}

struct PreparedPart
{
    std::string_view text;
    bool isOperator = false;
    bool isLiteral = false;
};

using PreparedParts = std::array<PreparedPart, k::maxPreparedParts>;

inline std::size_t prepareParts(std::string_view pattern, PreparedParts& parts) noexcept
{
    std::size_t numParts = 0;
    for (PatternCursor cursor(pattern); !cursor.exhausted(); cursor.advance())
    {
        if (numParts == parts.size())
        {
            return npos;
        }
        PreparedPart& part = parts[numParts++];
        part.isOperator = cursor.isOperator();
        part.text = part.isOperator ? std::string_view() : cursor.part();
        part.isLiteral = !part.isOperator && !hasOpener(part.text);
    }
    return numParts;
}

class PreparedCursor
{
public:
    PreparedCursor(const PreparedParts& parts, std::size_t numParts) noexcept
        : m_parts(parts.data())
        , m_numParts(numParts)
    {
    }

    bool exhausted() const noexcept
    {
        return m_index == m_numParts;
    }

    bool isOperator() const noexcept
    {
        return m_parts[m_index].isOperator;
    }

    bool matches(std::string_view addressPart) const noexcept
    {
        const PreparedPart& part = m_parts[m_index];
        return part.isLiteral ? part.text == addressPart : matchPart(part.text, addressPart);
    }

    void advance() noexcept
    {
        ++m_index;
    }

private:
    const PreparedPart* m_parts;
    std::size_t m_numParts;
    std::size_t m_index = 0;
};

inline std::vector<std::size_t> partEndsOf(std::string_view address)
{
    std::vector<std::size_t> ends;
    ends.reserve(static_cast<std::size_t>(std::count(address.begin() + 1, address.end(), k::partSeparator)) + 1);
    for (std::size_t i = 1; i < address.size(); ++i)
    {
        if (address[i] == k::partSeparator)
        {
            ends.push_back(i);
        }
    }
    ends.push_back(address.size());
    return ends;
}

class StoredAddressCursor
{
public:
    StoredAddressCursor(std::string_view address, const std::vector<std::size_t>& partEnds) noexcept
        : m_address(address)
        , m_partEnds(partEnds.data())
        , m_numParts(partEnds.size())
    {
    }

    bool exhausted() const noexcept
    {
        return m_index == m_numParts;
    }

    std::string_view part() const noexcept
    {
        const std::size_t start = m_index == 0 ? 1 : m_partEnds[m_index - 1] + 1;
        return m_address.substr(start, m_partEnds[m_index] - start);
    }

    void advance() noexcept
    {
        ++m_index;
    }

private:
    std::string_view m_address;
    const std::size_t* m_partEnds;
    std::size_t m_numParts;
    std::size_t m_index = 0;
};

}

namespace oscpm
{

/// The longest pattern a lookup memoises; a longer one is matched afresh
/// every time.
constexpr std::size_t kMaxMemoPatternLength = 256;

/// What `AddressSpace::dispatch` did: the number of methods visited, and
/// the fault that stopped the pattern parsing, in which case nothing was
/// visited.
struct DispatchResult
{
    std::size_t matched;
    std::optional<ParseError> error;
};

/// A set of OSC methods, each a well-formed address with a value of type
/// `T`, that a pattern is dispatched to. Addresses are kept in bytewise
/// order. A literal pattern is found by binary search; any other is
/// matched against every method, with the result memoised until the next
/// `add` or `remove` when `Memo` is true, as is the empty result of a
/// literal pattern that names no method. The memo has `1 << CacheBits`
/// entries and keeps a result of at most `InlineResults` methods for a
/// pattern of at most `kMaxMemoPatternLength` bytes, in a space of fewer
/// than 2^32 methods. `dispatch` looks a pattern up in the memo, and among
/// the registered addresses when moving a `T` cannot throw, before parsing
/// it; a lookup the memo does not hold is also faster then. `add` and `remove`
/// allocate; `lookup`, `dispatch` and `forEach` never do. A moved-from
/// space is empty
/// and usable, without a memo until it is assigned to. A visitor may call
/// `lookup` and `dispatch` on the space that called it. Not safe for
/// concurrent use.
template <typename T, bool Memo = true, unsigned CacheBits = 8, std::size_t InlineResults = 1024>
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
        std::vector<std::size_t> partEnds;
        if constexpr (kMethodMovesCannotThrow)
        {
            partEnds = detail::partEndsOf(address);
            if (m_partEnds.size() == m_partEnds.capacity())
            {
                m_partEnds.reserve(2 * m_partEnds.size() + 1);
            }
            if (m_hashes.size() == m_hashes.capacity())
            {
                m_hashes.reserve(2 * m_hashes.size() + 1);
            }
        }
        const auto offset = position - m_methods.begin();
        m_methods.insert(position, Method { std::string(address), std::move(value) });
        if constexpr (kMethodMovesCannotThrow)
        {
            m_partEnds.insert(m_partEnds.begin() + offset, std::move(partEnds));
            m_hashes.insert(m_hashes.begin() + offset, hashOf(address));
        }
        ++m_generation;
        insertIntoAddressIndex(static_cast<std::size_t>(offset));
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
        const auto offset = position - m_methods.begin();
        eraseFromAddressIndex(static_cast<std::size_t>(offset));
        m_methods.erase(position);
        if constexpr (kMethodMovesCannotThrow)
        {
            m_partEnds.erase(m_partEnds.begin() + offset);
            m_hashes.erase(m_hashes.begin() + offset);
        }
        if (m_addressIndex.empty())
        {
            rebuildAddressIndex();
        }
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

    /// Parses `pattern` and calls `visitor(std::string_view address, T& value)`
    /// for every method it matches, in bytewise address order, as
    /// `Pattern::parse` followed by `lookup`. A malformed pattern visits
    /// nothing and is reported. The visitor must not add or remove methods.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        std::size_t numVisited = 0;
        if (dispatchKnown(m_methods, pattern, visitor, numVisited))
        {
            return DispatchResult { numVisited, std::nullopt };
        }
        const ParseResult parsed = Pattern::parse(pattern);
        if (!parsed)
        {
            return DispatchResult { 0, parsed.error() };
        }
        return DispatchResult { lookupIn(m_methods, parsed.pattern(), visitor), std::nullopt };
    }

    /// Parses `pattern` and calls `visitor(std::string_view address, const T& value)`
    /// for every method it matches, in bytewise address order, as
    /// `Pattern::parse` followed by `lookup`. A malformed pattern visits
    /// nothing and is reported.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor) const
    {
        std::size_t numVisited = 0;
        if (dispatchKnown(m_methods, pattern, visitor, numVisited))
        {
            return DispatchResult { numVisited, std::nullopt };
        }
        const ParseResult parsed = Pattern::parse(pattern);
        if (!parsed)
        {
            return DispatchResult { 0, parsed.error() };
        }
        return DispatchResult { lookupIn(m_methods, parsed.pattern(), visitor), std::nullopt };
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
    static constexpr std::size_t kMinIndexSlots = 16;
    static constexpr std::size_t kMaxMemoisedMethods = std::numeric_limits<std::uint32_t>::max();

    struct Method
    {
        std::string address;
        T value;
    };

    static constexpr bool kMethodMovesCannotThrow = std::is_nothrow_move_constructible_v<Method> && std::is_nothrow_move_assignable_v<Method>;

    struct Bucket
    {
        std::uint64_t generation = 0;
        std::size_t patternLength = 0;
        std::size_t numResults = 0;
        std::size_t numReaders = 0;
        std::array<char, kMaxMemoPatternLength> pattern { };
        std::array<std::uint32_t, InlineResults> results { };
    };

    class BucketReader
    {
    public:
        explicit BucketReader(Bucket& bucket) noexcept
            : m_bucket(bucket)
        {
            ++m_bucket.numReaders;
        }

        ~BucketReader()
        {
            --m_bucket.numReaders;
        }

        BucketReader(const BucketReader&) = delete;
        BucketReader& operator=(const BucketReader&) = delete;

    private:
        Bucket& m_bucket;
    };

    static std::size_t hashOf(std::string_view text) noexcept
    {
        return std::hash<std::string_view> { }(text);
    }

    static std::size_t bucketIndex(std::size_t hash) noexcept
    {
        return hash & (kNumBuckets - 1);
    }

    void rebuildAddressIndex()
    {
        m_addressIndex.clear();
        if constexpr (!kMethodMovesCannotThrow)
        {
            return;
        }
        if (m_methods.size() > kMaxMemoisedMethods)
        {
            return;
        }
        std::size_t numSlots = kMinIndexSlots;
        while (numSlots < 2 * m_methods.size())
        {
            numSlots *= 2;
        }
        std::vector<std::uint32_t> slots(numSlots, 0U);
        for (std::size_t index = 0; index < m_hashes.size(); ++index)
        {
            std::size_t slot = m_hashes[index] & (numSlots - 1);
            while (slots[slot] != 0U)
            {
                slot = (slot + 1) & (numSlots - 1);
            }
            slots[slot] = static_cast<std::uint32_t>(index + 1);
        }
        m_addressIndex.swap(slots);
    }

    void insertIntoAddressIndex(std::size_t index)
    {
        if (m_addressIndex.size() < 2 * m_methods.size())
        {
            rebuildAddressIndex();
            return;
        }
        const auto shifted = static_cast<std::uint32_t>(index);
        for (std::uint32_t& slot : m_addressIndex)
        {
            slot += slot > shifted ? 1U : 0U;
        }
        const std::size_t mask = m_addressIndex.size() - 1;
        std::size_t slot = m_hashes[index] & mask;
        while (m_addressIndex[slot] != 0U)
        {
            slot = (slot + 1) & mask;
        }
        m_addressIndex[slot] = static_cast<std::uint32_t>(index + 1);
    }

    void eraseFromAddressIndex(std::size_t index)
    {
        if (m_addressIndex.empty())
        {
            return;
        }
        const std::size_t mask = m_addressIndex.size() - 1;
        std::size_t hole = m_hashes[index] & mask;
        while (m_addressIndex[hole] != index + 1)
        {
            hole = (hole + 1) & mask;
        }
        for (std::size_t next = (hole + 1) & mask; m_addressIndex[next] != 0U; next = (next + 1) & mask)
        {
            const std::size_t home = m_hashes[m_addressIndex[next] - 1] & mask;
            const std::size_t distanceFromHome = (next - home) & mask;
            const std::size_t distanceFromHole = (next - hole) & mask;
            if (distanceFromHome >= distanceFromHole)
            {
                m_addressIndex[hole] = m_addressIndex[next];
                hole = next;
            }
        }
        m_addressIndex[hole] = 0U;
        const auto removed = static_cast<std::uint32_t>(index + 1);
        for (std::uint32_t& slot : m_addressIndex)
        {
            slot -= slot > removed ? 1U : 0U;
        }
    }

    template <typename Methods, typename Visitor>
    bool dispatchKnown(Methods& methods, std::string_view text, Visitor& visitor, std::size_t& numVisited) const
    {
        const std::size_t hash = hashOf(text);
        if (!m_memo.empty() && text.size() <= kMaxMemoPatternLength)
        {
            Bucket& bucket = m_memo[bucketIndex(hash)];
            if (bucket.generation == m_generation && holds(bucket, text))
            {
                numVisited = visitMemoised(methods, bucket, visitor);
                return true;
            }
        }
        if (m_addressIndex.empty())
        {
            return false;
        }
        const std::size_t mask = m_addressIndex.size() - 1;
        for (std::size_t slot = hash & mask; m_addressIndex[slot] != 0U; slot = (slot + 1) & mask)
        {
            const std::size_t index = m_addressIndex[slot] - 1;
            if (m_hashes[index] == hash && methods[index].address == text)
            {
                visitor(std::string_view(methods[index].address), methods[index].value);
                numVisited = 1;
                return true;
            }
        }
        return false;
    }

    void rememberNoMatch(std::string_view text) const
    {
        if (m_memo.empty() || text.size() > kMaxMemoPatternLength || m_methods.size() > kMaxMemoisedMethods)
        {
            return;
        }
        Bucket& bucket = m_memo[bucketIndex(hashOf(text))];
        if (bucket.numReaders != 0)
        {
            return;
        }
        bucket.generation = m_generation;
        bucket.patternLength = text.size();
        bucket.numResults = 0;
        std::copy(text.begin(), text.end(), bucket.pattern.begin());
    }

    template <typename Methods, typename Visitor>
    static std::size_t visitMemoised(Methods& methods, Bucket& bucket, Visitor& visitor)
    {
        const BucketReader reader(bucket);
        for (std::size_t i = 0; i < bucket.numResults; ++i)
        {
            auto& method = methods[bucket.results[i]];
            visitor(std::string_view(method.address), method.value);
        }
        return bucket.numResults;
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
                rememberNoMatch(text);
                return 0;
            }
            visitor(std::string_view(position->address), position->value);
            return 1;
        }

        const bool memoisable = !m_memo.empty() && text.size() <= kMaxMemoPatternLength && methods.size() <= kMaxMemoisedMethods;
        Bucket* bucket = memoisable ? &m_memo[bucketIndex(hashOf(text))] : nullptr;
        if (bucket != nullptr && bucket->generation == m_generation && holds(*bucket, text))
        {
            return visitMemoised(methods, *bucket, visitor);
        }

        std::array<std::uint32_t, InlineResults> found { };
        std::size_t numFound = 0;
        detail::PreparedParts prepared;
        const std::size_t numPrepared = kMethodMovesCannotThrow ? detail::prepareParts(text, prepared) : detail::npos;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            auto& method = methods[index];
            const bool matched = numPrepared == detail::npos
                ? pattern.matches(method.address)
                : detail::matchParts(detail::PreparedCursor(prepared, numPrepared), detail::StoredAddressCursor(method.address, m_partEnds[index]));
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

        if (bucket != nullptr && bucket->numReaders == 0 && numFound <= InlineResults)
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
    std::vector<std::vector<std::size_t>> m_partEnds;
    std::vector<std::size_t> m_hashes;
    std::vector<std::uint32_t> m_addressIndex;
    mutable std::vector<Bucket> m_memo;
    std::uint64_t m_generation = 1;
};

}
