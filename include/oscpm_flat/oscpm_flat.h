/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

// oscpm_flat: OSC address pattern matching by a flat loop over the
// registered methods, a Glushkov automaton per wildcard part, and a memo
// cache.
//
// Within a class, "x-y" is a range whenever a '-' sits between two
// characters, so "[--a]" is a range and "[a--]" is a reversed (empty) range;
// "[]" matches nothing and "[!]" any byte; ']' '}' ',' outside a construct
// are literal; '?' and '*' match any byte but '/'; matching is byte-wise. An
// empty part is the OSC 1.1 "//" operator, consecutive ones collapse, and a
// trailing '/' is therefore a trailing "//".
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace oscpm_flat
{

constexpr std::size_t kMaxParts = 32;
constexpr std::size_t kMaxPositions = 128; // per pattern, all Wild parts together, plus sentinels
constexpr std::size_t kMaxAltProduct = 16;
constexpr std::size_t kAltBuffer = 512;
constexpr std::size_t kMaxPatternBytes = 256; // what the cache will copy
constexpr std::size_t kInlineResults = 32; // results a cache entry holds inline

enum class Error : std::uint8_t
{
    None,
    MissingLeadingSlash,
    UnterminatedClass,
    UnterminatedBraces,
    NestedBraces,
    TooManyParts,
    TooManyPositions,
    PatternTooLong,
    IllegalByte
};

// A set of automaton positions: one word when a part has at most 64
// positions, two beyond that.
struct Mask
{
    std::uint64_t w[2];
    static Mask none()
    {
        Mask m;
        m.w[0] = m.w[1] = 0;
        return m;
    }
    void set(unsigned i) { w[i >> 6] |= std::uint64_t { 1 } << (i & 63); }
    bool test(unsigned i) const { return (w[i >> 6] >> (i & 63)) & 1u; }
    bool any() const { return (w[0] | w[1]) != 0; }
    Mask& operator|=(const Mask& o)
    {
        w[0] |= o.w[0];
        w[1] |= o.w[1];
        return *this;
    }
    bool intersects(const Mask& o) const { return ((w[0] & o.w[0]) | (w[1] & o.w[1])) != 0; }
};

struct Wild
{
    std::uint16_t base; // first pool index of this part's positions
    std::uint16_t n; // positions including the sentinel, which is local index n-1
    Mask first; // positions that may consume the first byte (or the sentinel if nullable)
    Mask final; // the sentinel alone
    std::size_t minlen;
    std::size_t maxlen; // SIZE_MAX when a '*' is present
};

// A character class: 256 bits, one per byte value.
struct Cls
{
    std::uint64_t w[4];
    bool test(unsigned char c) const { return (w[c >> 6] >> (c & 63)) & 1u; }
    void set(unsigned char c) { w[c >> 6] |= std::uint64_t { 1 } << (c & 63); }
    static Cls none()
    {
        Cls x;
        x.w[0] = x.w[1] = x.w[2] = x.w[3] = 0;
        return x;
    }
    static Cls one(unsigned char c)
    {
        Cls x = none();
        x.set(c);
        return x;
    }
    static Cls anyButSlash()
    {
        Cls x;
        x.w[0] = x.w[1] = x.w[2] = x.w[3] = ~std::uint64_t { 0 };
        x.w['/' >> 6] &= ~(std::uint64_t { 1 } << ('/' & 63));
        return x;
    }
};

struct Part
{
    enum Kind : std::uint8_t
    {
        Literal,
        Alt,
        Wild,
        Slash2
    } kind;
    std::string_view lit; // Literal: a view of the caller's pattern text
    std::uint16_t altOff[kMaxAltProduct]; // Alt: offsets into the pattern's alt buffer
    std::uint16_t altLen[kMaxAltProduct];
    std::uint8_t nalts;
    oscpm_flat::Wild wild; // Wild
};

class Pattern
{
public:
    Pattern() = default;
    explicit Pattern(std::string_view text) { compile(text); }

    bool valid() const { return m_error == Error::None; }
    Error error() const { return m_error; }
    bool literal() const { return m_literal; } // every part is Literal: exact-map candidate
    std::string_view text() const { return m_text; }
    std::size_t nparts() const { return m_nparts; }
    const Part& part(std::size_t i) const { return m_parts[i]; }

    // One byte through one Wild part: the positions that consume `c`, mapped through follow.
    Mask step(const oscpm_flat::Wild& w, const Mask& active, unsigned char c) const
    {
        Mask next = Mask::none();
        const Cls* cls = m_cls + w.base;
        const Mask* follow = m_follow + w.base;
        std::uint64_t s = active.w[0];
        while (s)
        {
            const unsigned p = ctz(s);
            if (cls[p].test(c))
                next |= follow[p];
            s &= s - 1;
        }
        s = active.w[1];
        while (s)
        {
            const unsigned p = 64 + ctz(s);
            if (cls[p].test(c))
                next |= follow[p];
            s &= s - 1;
        }
        return next;
    }

    bool matchPart(const Part& p, std::string_view seg) const
    {
        switch (p.kind)
        {
        case Part::Literal:
            return seg == p.lit;
        case Part::Alt:
            for (std::uint8_t i = 0; i < p.nalts; ++i)
                if (seg == std::string_view(m_altBuffer + p.altOff[i], p.altLen[i]))
                    return true;
            return false;
        case Part::Wild:
        {
            const oscpm_flat::Wild& w = p.wild;
            if (seg.size() < w.minlen || seg.size() > w.maxlen)
                return false;
            Mask a = w.first;
            for (const char c : seg)
            {
                a = step(w, a, static_cast<unsigned char>(c));
                if (!a.any())
                    return false;
            }
            return a.intersects(w.final);
        }
        case Part::Slash2:
            return false;
        }
        return false;
    }

    // Two-pointer walk over parts with "//" in the role of '*'. `seg(j)` yields address part j.
    template <typename Segs>
    bool matchAddress(std::string_view address, const Segs& segs) const
    {
        const std::size_t na = segs.nparts();
        std::size_t i = 0, j = 0;
        std::size_t si = SIZE_MAX, sj = 0;
        while (j < na)
        {
            if (i < m_nparts && m_parts[i].kind == Part::Slash2)
            {
                si = i;
                sj = j;
                ++i;
                continue;
            }
            if (i < m_nparts && matchPart(m_parts[i], segs.seg(address, j)))
            {
                ++i;
                ++j;
                continue;
            }
            if (si != SIZE_MAX)
            {
                i = si + 1;
                j = ++sj;
                continue;
            }
            return false;
        }
        while (i < m_nparts && m_parts[i].kind == Part::Slash2)
            ++i;
        return i == m_nparts;
    }

private:
    static unsigned ctz(std::uint64_t x)
    {
#if defined(_MSC_VER) && !defined(__clang__)
        unsigned long index = 0;
        _BitScanForward64(&index, x);
        return static_cast<unsigned>(index);
#else
        return static_cast<unsigned>(__builtin_ctzll(x));
#endif
    }

    struct Frag
    {
        Mask first, last;
        bool nullable;
        std::size_t minlen, maxlen;
    };
    static Frag emptyFrag()
    {
        Frag f;
        f.first = Mask::none();
        f.last = Mask::none();
        f.nullable = true;
        f.minlen = 0;
        f.maxlen = 0;
        return f;
    }

    static std::size_t addLen(std::size_t a, std::size_t b) { return a == SIZE_MAX || b == SIZE_MAX ? SIZE_MAX : a + b; }

    // Appends element `e` to sequence `acc` (Glushkov concatenation).
    void concat(Frag& acc, const Frag& e)
    {
        for (unsigned k = 0; k < 2; ++k)
        {
            std::uint64_t s = acc.last.w[k];
            while (s)
            {
                m_follow[m_npos_base + 64 * k + ctz(s)] |= e.first;
                s &= s - 1;
            }
        }
        if (acc.nullable)
            acc.first |= e.first;
        Mask last = e.last;
        if (e.nullable)
            last |= acc.last;
        acc.last = last;
        acc.nullable = acc.nullable && e.nullable;
        acc.minlen += e.minlen;
        acc.maxlen = addLen(acc.maxlen, e.maxlen);
    }

    // Allocates a position with class `cls`; returns its local index or fails.
    bool position(const Cls& cls, unsigned& local)
    {
        if (m_npos >= kMaxPositions)
        {
            m_error = Error::TooManyPositions;
            return false;
        }
        local = static_cast<unsigned>(m_npos - m_npos_base);
        if (local >= 128)
        {
            m_error = Error::TooManyPositions;
            return false;
        }
        m_cls[m_npos] = cls;
        m_follow[m_npos] = Mask::none();
        ++m_npos;
        return true;
    }

    static Frag single(unsigned local, bool star)
    {
        Frag f = emptyFrag();
        f.first.set(local);
        f.last.set(local);
        f.nullable = star;
        f.minlen = star ? 0 : 1;
        f.maxlen = star ? SIZE_MAX : 1;
        return f;
    }

    // Glob class semantics: "x-y" is a range when '-' sits between two characters
    // that are not the closing bracket; a leading '!' negates.
    static Cls classOf(std::string_view body)
    {
        Cls b = Cls::none();
        bool negate = false;
        std::size_t i = 0;
        if (!body.empty() && body[0] == '!')
        {
            negate = true;
            i = 1;
        }
        while (i < body.size())
        {
            const auto lo = static_cast<unsigned char>(body[i]);
            if (i + 2 < body.size() && body[i + 1] == '-')
            {
                const auto hi = static_cast<unsigned char>(body[i + 2]);
                for (unsigned c = lo; c <= hi; ++c)
                    b.set(static_cast<unsigned char>(c)); // reversed range sets nothing
                i += 3;
            }
            else
            {
                b.set(lo);
                ++i;
            }
        }
        if (negate)
        {
            for (std::uint64_t& w : b.w)
                w = ~w;
            b.w['/' >> 6] &= ~(std::uint64_t { 1 } << ('/' & 63));
        }
        return b;
    }

    bool literalRun(std::string_view text, Frag& acc)
    {
        for (const char c : text)
        {
            unsigned p;
            if (!position(Cls::one(static_cast<unsigned char>(c)), p))
                return false;
            concat(acc, single(p, false));
        }
        return true;
    }

    bool compileWild(std::string_view seg, Part& part)
    {
        part.kind = Part::Wild;
        m_npos_base = m_npos;
        Frag acc = emptyFrag();
        std::size_t i = 0;
        while (i < seg.size())
        {
            const char c = seg[i];
            if (c == '*' || c == '?')
            {
                unsigned p;
                if (!position(Cls::anyButSlash(), p))
                    return false;
                if (c == '*')
                    m_follow[m_npos_base + p].set(p);
                concat(acc, single(p, c == '*'));
                ++i;
            }
            else if (c == '[')
            {
                const std::size_t close = seg.find(']', i + 1 + (i + 1 < seg.size() && seg[i + 1] == '!' ? 1 : 0));
                if (close == std::string_view::npos)
                {
                    m_error = Error::UnterminatedClass;
                    return false;
                }
                unsigned p;
                if (!position(classOf(seg.substr(i + 1, close - i - 1)), p))
                    return false;
                concat(acc, single(p, false));
                i = close + 1;
            }
            else if (c == '{')
            {
                const std::size_t close = seg.find('}', i + 1);
                if (close == std::string_view::npos)
                {
                    m_error = Error::UnterminatedBraces;
                    return false;
                }
                const std::string_view body = seg.substr(i + 1, close - i - 1);
                if (body.find('{') != std::string_view::npos)
                {
                    m_error = Error::NestedBraces;
                    return false;
                }
                Frag group = emptyFrag();
                group.nullable = false;
                group.minlen = SIZE_MAX;
                std::string_view rest = body;
                for (;;)
                {
                    const std::size_t comma = rest.find(',');
                    const std::string_view alt = rest.substr(0, comma);
                    Frag a = emptyFrag();
                    if (!literalRun(alt, a))
                        return false;
                    group.first |= a.first;
                    group.last |= a.last;
                    group.nullable = group.nullable || a.nullable;
                    group.minlen = a.minlen < group.minlen ? a.minlen : group.minlen;
                    group.maxlen = a.maxlen > group.maxlen ? a.maxlen : group.maxlen;
                    if (comma == std::string_view::npos)
                        break;
                    rest = rest.substr(comma + 1);
                }
                concat(acc, group);
                i = close + 1;
            }
            else
            {
                unsigned p;
                if (!position(Cls::one(static_cast<unsigned char>(c)), p))
                    return false;
                concat(acc, single(p, false));
                ++i;
            }
        }
        // Sentinel end position: reached from every last position, and from the start when nullable.
        unsigned end;
        if (!position(Cls::none(), end))
            return false;
        Frag endFrag = single(end, false);
        concat(acc, endFrag);
        part.wild.base = static_cast<std::uint16_t>(m_npos_base);
        part.wild.n = static_cast<std::uint16_t>(m_npos - m_npos_base);
        part.wild.first = acc.first;
        part.wild.final = Mask::none();
        part.wild.final.set(end);
        part.wild.minlen = acc.minlen - 1; // the sentinel counted one
        part.wild.maxlen = acc.maxlen == SIZE_MAX ? SIZE_MAX : acc.maxlen - 1;
        return true;
    }

    // Braces only, no wildcards: expand the cross product into the alt buffer if it is small enough.
    bool tryAlt(std::string_view seg, Part& part)
    {
        std::string_view groups[8];
        std::size_t ngroups = 0;
        std::string_view fixed[9]; // literal text between groups
        std::size_t product = 1, i = 0, start = 0;
        while (i < seg.size())
        {
            if (seg[i] == '{')
            {
                const std::size_t close = seg.find('}', i + 1);
                if (close == std::string_view::npos || ngroups == 8)
                    return false;
                fixed[ngroups] = seg.substr(start, i - start);
                groups[ngroups] = seg.substr(i + 1, close - i - 1);
                if (groups[ngroups].find('{') != std::string_view::npos)
                    return false; // nested: compileWild reports it
                std::size_t members = 1;
                for (const char c : groups[ngroups])
                    if (c == ',')
                        ++members;
                product *= members;
                if (product > kMaxAltProduct)
                    return false;
                ++ngroups;
                i = close + 1;
                start = i;
            }
            else
                ++i;
        }
        fixed[ngroups] = seg.substr(start);
        // Enumerate the product into the buffer.
        std::size_t idx[8] = { };
        part.kind = Part::Alt;
        part.nalts = 0;
        for (;;)
        {
            char* out = m_altBuffer + m_altUsed;
            std::size_t len = 0;
            auto append = [&](std::string_view s)
            { if (m_altUsed + len + s.size() > kAltBuffer) return false; std::memcpy(out + len, s.data(), s.size()); len += s.size(); return true; };
            for (std::size_t g = 0; g <= ngroups; ++g)
            {
                if (!append(fixed[g]))
                    return false;
                if (g < ngroups)
                {
                    std::string_view rest = groups[g];
                    for (std::size_t k = 0; k < idx[g]; ++k)
                        rest = rest.substr(rest.find(',') + 1);
                    if (!append(rest.substr(0, rest.find(','))))
                        return false;
                }
            }
            part.altOff[part.nalts] = static_cast<std::uint16_t>(m_altUsed);
            part.altLen[part.nalts] = static_cast<std::uint16_t>(len);
            ++part.nalts;
            m_altUsed += len;
            std::size_t g = 0;
            for (; g < ngroups; ++g)
            {
                std::size_t members = 1;
                for (const char c : groups[g])
                    if (c == ',')
                        ++members;
                if (++idx[g] < members)
                    break;
                idx[g] = 0;
            }
            if (g == ngroups)
                break;
        }
        return true;
    }

    void compile(std::string_view text)
    {
        m_text = text;
        m_error = Error::None;
        m_nparts = 0;
        m_npos = 0;
        m_altUsed = 0;
        m_literal = true;
        if (text.empty() || text[0] != '/')
        {
            m_error = Error::MissingLeadingSlash;
            return;
        }
        if (text.size() > kMaxPatternBytes)
        {
            m_error = Error::PatternTooLong;
            return;
        }
        for (const char c : text)
        {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x21 || u > 0x7E)
            {
                m_error = Error::IllegalByte;
                return;
            }
        }
        std::size_t i = 1;
        for (;;)
        {
            const std::size_t slash = text.find('/', i);
            const std::string_view seg = text.substr(i, slash == std::string_view::npos ? std::string_view::npos : slash - i);
            if (seg.empty())
            {
                if (m_nparts == 0 || m_parts[m_nparts - 1].kind != Part::Slash2)
                {
                    if (m_nparts == kMaxParts)
                    {
                        m_error = Error::TooManyParts;
                        return;
                    }
                    m_parts[m_nparts++].kind = Part::Slash2;
                }
                m_literal = false;
            }
            else
            {
                if (m_nparts == kMaxParts)
                {
                    m_error = Error::TooManyParts;
                    return;
                }
                Part& part = m_parts[m_nparts++];
                part.nalts = 0;
                const bool wild = seg.find_first_of("*?[") != std::string_view::npos;
                const bool braces = seg.find('{') != std::string_view::npos;
                if (!wild && !braces)
                {
                    part.kind = Part::Literal;
                    part.lit = seg;
                }
                else
                {
                    m_literal = false;
                    if (!(!wild && braces && tryAlt(seg, part)))
                    {
                        if (!compileWild(seg, part))
                            return;
                    }
                }
            }
            if (slash == std::string_view::npos)
                break;
            i = slash + 1;
        }
    }

    std::string_view m_text;
    Error m_error = Error::MissingLeadingSlash;
    bool m_literal = false;
    std::size_t m_nparts = 0;
    Part m_parts[kMaxParts];
    std::size_t m_npos = 0, m_npos_base = 0;
    Cls m_cls[kMaxPositions]; // pools are left uninitialised; only [0, m_npos) is ever read
    Mask m_follow[kMaxPositions];
    char m_altBuffer[kAltBuffer];
    std::size_t m_altUsed = 0;
};

// The part boundaries of one address, precomputed once and applied to the
// text at use, so a stored table never dangles when the string moves.
class Segments
{
public:
    Segments()
        : m_n(0)
    {
    }
    explicit Segments(std::string_view address) { reset(address); }
    void reset(std::string_view address)
    {
        m_n = 0;
        if (address.empty())
            return;
        std::size_t i = address[0] == '/' ? 1 : 0;
        m_start[m_n] = static_cast<std::uint16_t>(i);
        for (; i < address.size(); ++i)
            if (address[i] == '/' && m_n + 1 < kMaxParts)
            {
                m_end[m_n++] = static_cast<std::uint16_t>(i);
                m_start[m_n] = static_cast<std::uint16_t>(i + 1);
            }
        m_end[m_n++] = static_cast<std::uint16_t>(address.size());
    }
    std::size_t nparts() const { return m_n; }
    std::string_view seg(std::string_view text, std::size_t j) const { return text.substr(m_start[j], m_end[j] - m_start[j]); }

private:
    std::uint16_t m_start[kMaxParts + 1], m_end[kMaxParts + 1];
    std::size_t m_n;
};

inline bool isValidAddress(std::string_view a)
{
    if (a.empty() || a[0] != '/' || a.back() == '/')
        return false;
    for (std::size_t i = 1; i < a.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(a[i]);
        if (c < 0x21 || c > 0x7E || std::strchr("#*,?[]{}", static_cast<char>(c)))
            return false;
        if (a[i] == '/' && a[i - 1] == '/')
            return false;
    }
    return true;
}

// One-shot convenience: compile and match against one address.
inline bool match(std::string_view pattern, std::string_view address)
{
    const Pattern p(pattern);
    return p.valid() && p.matchAddress(address, Segments(address));
}

template <typename T, unsigned CacheBits = 8, std::size_t InlineResults = kInlineResults>
class Registry
{
public:
    using MethodId = std::uint32_t;
    struct Method
    {
        std::string address;
        Segments segs;
        T value;
    };

    bool add(std::string_view address, T value)
    {
        if (!isValidAddress(address) || find(address) != m_index.end())
            return false;
        m_methods.push_back(Method { std::string(address), Segments(address), std::move(value) });
        m_index.emplace(fnv(address), static_cast<MethodId>(m_methods.size() - 1));
        ++m_generation;
        return true;
    }
    bool remove(std::string_view address)
    {
        const auto it = find(address);
        if (it == m_index.end())
            return false;
        const MethodId id = it->second, last = static_cast<MethodId>(m_methods.size() - 1);
        m_index.erase(it);
        if (id != last)
        {
            const auto moved = find(m_methods[last].address);
            m_methods[id] = std::move(m_methods[last]);
            moved->second = id;
        }
        m_methods.pop_back();
        ++m_generation;
        return true;
    }
    std::size_t size() const { return m_methods.size(); }
    void invalidateCache() { ++m_generation; } // every cached entry now misses

    struct Result
    {
        std::size_t matched;
        bool malformed;
    };

    // Calls visitor(address, value) for each method the pattern reaches.
    template <typename Visitor>
    Result dispatch(std::string_view pattern, Visitor&& visitor)
    {
        if (pattern.find_first_of("*?[{") == std::string_view::npos && pattern.find("//") == std::string_view::npos
            && !pattern.empty() && pattern.back() != '/')
        {
            const auto it = find(pattern);
            if (it == m_index.end())
                return { 0, false };
            visitor(m_methods[it->second].address, m_methods[it->second].value);
            return { 1, false };
        }
        Entry* hit = lookup(pattern);
        if (hit)
        {
            for (std::uint16_t k = 0; k < hit->count; ++k)
                visitor(m_methods[hit->ids[k]].address, m_methods[hit->ids[k]].value);
            return { hit->count, false };
        }
        const Pattern p(pattern);
        if (!p.valid())
            return { 0, true };
        MethodId ids[InlineResults];
        std::size_t n = 0;
        bool cacheable = pattern.size() <= kMaxPatternBytes;
        for (MethodId id = 0; id < m_methods.size(); ++id)
        {
            if (p.matchAddress(m_methods[id].address, m_methods[id].segs))
            {
                visitor(m_methods[id].address, m_methods[id].value);
                if (n < InlineResults)
                    ids[n] = id;
                else
                    cacheable = false;
                ++n;
            }
        }
        if (cacheable)
            insert(pattern, ids, static_cast<std::uint16_t>(n));
        return { n, false };
    }

private:
    struct Entry
    {
        char bytes[kMaxPatternBytes];
        std::uint16_t len = 0;
        bool used = false;
        std::uint64_t generation = 0;
        MethodId ids[InlineResults];
        std::uint16_t count = 0;
    };
    struct Bucket
    {
        Entry way[2];
        std::uint8_t older = 0;
    };

    static std::uint64_t fnv(std::string_view s)
    {
        std::uint64_t h = 1469598103934665603ull;
        for (const char c : s)
        {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ull;
        }
        return h;
    }
    static std::size_t hash(std::string_view s) { return static_cast<std::size_t>(fnv(s) >> (64 - CacheBits)); }
    // The exact index is keyed by hash and verified against the stored address, so a lookup builds no string.
    using Index = std::unordered_multimap<std::uint64_t, MethodId>;
    typename Index::iterator find(std::string_view address)
    {
        const auto range = m_index.equal_range(fnv(address));
        for (auto it = range.first; it != range.second; ++it)
            if (m_methods[it->second].address == address)
                return it;
        return m_index.end();
    }
    Entry* lookup(std::string_view pattern)
    {
        if (pattern.size() > kMaxPatternBytes)
            return nullptr;
        Bucket& b = m_cache[hash(pattern)];
        for (Entry& e : b.way)
            if (e.used && e.generation == m_generation && e.len == pattern.size() && std::memcmp(e.bytes, pattern.data(), e.len) == 0)
                return &e;
        return nullptr;
    }
    void insert(std::string_view pattern, const MethodId* ids, std::uint16_t n)
    {
        Bucket& b = m_cache[hash(pattern)];
        Entry& e = b.way[b.older];
        b.older ^= 1;
        std::memcpy(e.bytes, pattern.data(), pattern.size());
        e.len = static_cast<std::uint16_t>(pattern.size());
        e.used = true;
        e.generation = m_generation;
        std::memcpy(e.ids, ids, n * sizeof(MethodId));
        e.count = n;
    }

    Index m_index;
    std::vector<Method> m_methods;
    std::uint64_t m_generation = 1;
    std::vector<Bucket> m_cache = std::vector<Bucket>(std::size_t { 1 } << CacheBits);
};

} // namespace oscpm_flat
