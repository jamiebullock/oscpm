/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/error.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace oscpm::detail
{

template <unsigned CacheBits, std::size_t InlineResults>
class DispatchMemo
{
public:
    using Results = std::array<std::uint32_t, InlineResults>;

    static constexpr std::size_t kInlineResults = InlineResults;

    DispatchMemo()
        : m_buckets(kNumBuckets)
    {
    }

    DispatchMemo(const DispatchMemo& other)
        : m_buckets(other.m_buckets)
        , m_generation(other.m_generation)
    {
        forgetReaders();
    }

    DispatchMemo(DispatchMemo&&) noexcept = default;

    DispatchMemo& operator=(const DispatchMemo& other)
    {
        if (this == &other)
        {
            return *this;
        }
        m_buckets = other.m_buckets;
        m_generation = other.m_generation;
        forgetReaders();
        return *this;
    }

    DispatchMemo& operator=(DispatchMemo&&) noexcept = default;

    bool accepts(std::string_view text) const noexcept
    {
        return !m_buckets.empty() && text.size() <= kMaxMemoPatternLength;
    }

    template <typename Methods, typename Visitor>
    bool visit(std::string_view text, std::size_t hash, Methods& methods, Visitor& visitor, std::size_t& numVisited)
    {
        Bucket& bucket = bucketFor(hash);
        if (bucket.generation != m_generation || !holds(bucket, text))
        {
            return false;
        }
        const BucketReader reader(bucket);
        for (std::size_t i = 0; i < bucket.numResults; ++i)
        {
            auto& method = methods[bucket.results[i]];
            visitor(std::string_view(method.address), method.value);
        }
        numVisited = bucket.numResults;
        return true;
    }

    void store(std::string_view text, std::size_t hash, const Results& results, std::size_t numResults) noexcept
    {
        Bucket& bucket = bucketFor(hash);
        if (bucket.numReaders != 0)
        {
            return;
        }
        remember(bucket, text, numResults);
        std::copy_n(results.begin(), numResults, bucket.results.begin());
    }

    void rememberNoMatch(std::string_view text, std::size_t hash) noexcept
    {
        Bucket& bucket = bucketFor(hash);
        if (bucket.numReaders != 0)
        {
            return;
        }
        remember(bucket, text, 0);
    }

    void forget() noexcept
    {
        ++m_generation;
    }

    void ensureBuckets()
    {
        if (m_buckets.empty())
        {
            m_buckets.resize(kNumBuckets);
        }
    }

private:
    static constexpr std::size_t kNumBuckets = std::size_t { 1 } << CacheBits;
    static constexpr std::uint64_t kNeverStored = 0;

    struct Bucket
    {
        std::uint64_t generation = kNeverStored;
        std::size_t patternLength = 0;
        std::size_t numResults = 0;
        std::size_t numReaders = 0;
        std::array<char, kMaxMemoPatternLength> pattern { };
        Results results { };
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

    Bucket& bucketFor(std::size_t hash) noexcept
    {
        return m_buckets[hash & (kNumBuckets - 1)];
    }

    void forgetReaders() noexcept
    {
        for (Bucket& bucket : m_buckets)
        {
            bucket.numReaders = 0;
        }
    }

    static bool holds(const Bucket& bucket, std::string_view text) noexcept
    {
        return bucket.patternLength == text.size() && std::string_view(bucket.pattern.data(), bucket.patternLength) == text;
    }

    void remember(Bucket& bucket, std::string_view text, std::size_t numResults) noexcept
    {
        bucket.generation = m_generation;
        bucket.patternLength = text.size();
        bucket.numResults = numResults;
        std::copy(text.begin(), text.end(), bucket.pattern.begin());
    }

    std::vector<Bucket> m_buckets;
    std::uint64_t m_generation = kNeverStored + 1;
};

class NoDispatchMemo
{
public:
    using Results = std::array<std::uint32_t, 0>;

    static constexpr std::size_t kInlineResults = 0;

    constexpr bool accepts(std::string_view) const noexcept
    {
        return false;
    }

    template <typename Methods, typename Visitor>
    constexpr bool visit(std::string_view, std::size_t, Methods&, Visitor&, std::size_t&) const noexcept
    {
        return false;
    }

    constexpr void store(std::string_view, std::size_t, const Results&, std::size_t) const noexcept
    {
    }

    constexpr void rememberNoMatch(std::string_view, std::size_t) const noexcept
    {
    }

    constexpr void forget() const noexcept
    {
    }

    constexpr void ensureBuckets() const noexcept
    {
    }
};

}
