/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oscpm_regex
{

/// Why a pattern failed to compile. `RegexRejected` is a translation the
/// regex engine refused.
enum class Error : std::uint8_t
{
    None,
    MissingLeadingSlash,
    IllegalByte,
    UnterminatedClass,
    UnterminatedBraces,
    NestedBraces,
    RegexRejected
};

}

namespace oscpm_regex::detail
{

inline void hex(std::string& out, unsigned char c)
{
    char buf[5];
    std::snprintf(buf, sizeof buf, "\\x%02X", c);
    out += buf;
}

inline void literal(std::string& out, char c) { hex(out, static_cast<unsigned char>(c)); }

inline void classSet(std::string_view body, bool (&set)[256])
{
    std::memset(set, 0, sizeof set);
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
                set[c] = true;
            i += 3;
        }
        else
        {
            set[lo] = true;
            ++i;
        }
    }
    if (negate)
    {
        for (bool& b : set)
            b = !b;
        set[static_cast<unsigned char>('/')] = false;
    }
}

inline void appendClass(std::string& out, const bool (&set)[256])
{
    out += '[';
    bool any = false;
    for (unsigned c = 0; c < 256;)
    {
        if (!set[c])
        {
            ++c;
            continue;
        }
        unsigned d = c;
        while (d + 1 < 256 && set[d + 1])
            ++d;
        hex(out, static_cast<unsigned char>(c));
        if (d > c)
        {
            out += '-';
            hex(out, static_cast<unsigned char>(d));
        }
        any = true;
        c = d + 1;
    }
    if (!any)
        out += "^\\x00-\\xFF"; // a class that no byte is in
    out += ']';
}

inline Error translatePart(std::string_view seg, std::string& out)
{
    std::size_t i = 0;
    while (i < seg.size())
    {
        const char c = seg[i];
        if (c == '?')
        {
            out += "[^/]";
            ++i;
        }
        else if (c == '*')
        {
            out += "[^/]*";
            ++i;
        }
        else if (c == '[')
        {
            const std::size_t close = seg.find(']', i + 1 + (i + 1 < seg.size() && seg[i + 1] == '!' ? 1 : 0));
            if (close == std::string_view::npos)
                return Error::UnterminatedClass;
            bool set[256];
            classSet(seg.substr(i + 1, close - i - 1), set);
            appendClass(out, set);
            i = close + 1;
        }
        else if (c == '{')
        {
            const std::size_t close = seg.find('}', i + 1);
            if (close == std::string_view::npos)
                return Error::UnterminatedBraces;
            const std::string_view body = seg.substr(i + 1, close - i - 1);
            if (body.find('{') != std::string_view::npos)
                return Error::NestedBraces;
            out += "(?:";
            for (const char b : body)
            {
                if (b == ',')
                    out += '|';
                else
                    literal(out, b);
            }
            out += ')';
            i = close + 1;
        }
        else
        {
            literal(out, c);
            ++i;
        }
    }
    return Error::None;
}

inline Error translate(std::string_view pattern, std::string& out)
{
    out.clear();
    if (pattern.empty() || pattern[0] != '/')
        return Error::MissingLeadingSlash;
    for (const char c : pattern)
    {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x21 || u > 0x7E)
            return Error::IllegalByte;
    }
    std::size_t i = 1;
    bool lastWasSlash2 = false;
    for (;;)
    {
        const std::size_t slash = pattern.find('/', i);
        const std::string_view seg = pattern.substr(i, slash == std::string_view::npos ? std::string_view::npos : slash - i);
        if (seg.empty())
        {
            if (!lastWasSlash2)
                out += "(?:/[^/]*)*";
            lastWasSlash2 = true;
        }
        else
        {
            out += '/';
            if (const Error e = translatePart(seg, out); e != Error::None)
                return e;
            lastWasSlash2 = false;
        }
        if (slash == std::string_view::npos)
            break;
        i = slash + 1;
    }
    return Error::None;
}

struct TransparentHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view> { }(s); }
    std::size_t operator()(const std::string& s) const noexcept { return std::hash<std::string_view> { }(s); }
};

}

namespace oscpm_regex
{

/// Whether `a` is a well-formed OSC address: a leading '/', no trailing or
/// doubled '/', and only printable ASCII other than `#*,?[]{}`.
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

/// An OSC address pattern compiled to a regular expression. Construction
/// allocates, and so does every call to `matches`.
class Pattern
{
public:
    Pattern() = default;

    /// Compiles `text`; `valid` and `error` report the outcome.
    explicit Pattern(std::string_view text)
    {
        std::string rx;
        m_error = detail::translate(text, rx);
        if (m_error != Error::None)
            return;
        try
        {
            m_re = std::regex(rx, std::regex::ECMAScript);
        }
        catch (const std::regex_error&)
        {
            m_error = Error::RegexRejected;
        }
    }

    /// Whether the pattern compiled.
    bool valid() const { return m_error == Error::None; }

    /// Why the pattern did not compile, or `Error::None`.
    Error error() const { return m_error; }

    /// Whether this pattern matches `address`, byte for byte. False for an
    /// invalid pattern, for an address without a leading '/', and for an
    /// input the regex engine gives up on.
    bool matches(std::string_view address) const
    {
        if (!valid() || address.empty() || address[0] != '/')
            return false;
        try
        {
            return std::regex_match(address.data(), address.data() + address.size(), m_re);
        }
        catch (const std::regex_error&)
        {
            return false;
        } // error_complexity or error_stack
    }

private:
    std::regex m_re;
    Error m_error = Error::MissingLeadingSlash;
};

/// Compiles `pattern` and tests it against `address`.
inline bool match(std::string_view pattern, std::string_view address) { return Pattern(pattern).matches(address); }

/// A set of methods, each a well-formed address with a value of type `T`,
/// that a pattern is dispatched to. An exact address and a pattern seen
/// since the last `add` or `remove` are served from hash maps without
/// allocating; any other pattern is compiled and matched against every
/// method, which allocates. Not safe for concurrent use.
template <typename T>
class Registry
{
public:
    using MethodId = std::uint32_t;

    /// How many methods a dispatch visited, and whether the pattern was
    /// malformed, in which case it visited none.
    struct Result
    {
        std::size_t matched;
        bool malformed;
    };

    /// Registers `value` under `address`; false if the address is malformed
    /// or already registered.
    bool add(std::string_view address, T value)
    {
        if (!isValidAddress(address) || m_index.find(address) != m_index.end())
            return false;
        m_methods.push_back(Method { std::string(address), std::move(value) });
        m_index.emplace(m_methods.back().address, static_cast<MethodId>(m_methods.size() - 1));
        m_cache.clear();
        return true;
    }

    /// Unregisters `address`; false if it is not registered.
    bool remove(std::string_view address)
    {
        const auto it = m_index.find(address);
        if (it == m_index.end())
            return false;
        const MethodId id = it->second, last = static_cast<MethodId>(m_methods.size() - 1);
        m_index.erase(it);
        if (id != last)
        {
            m_methods[id] = std::move(m_methods[last]);
            m_index.find(m_methods[id].address)->second = id;
        }
        m_methods.pop_back();
        m_cache.clear();
        return true;
    }

    /// The number of registered methods.
    std::size_t size() const { return m_methods.size(); }

    /// Forgets every cached pattern result.
    void invalidateCache() { m_cache.clear(); }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches. The visitor must not add or remove methods.
    template <typename Visitor>
    Result dispatch(std::string_view pattern, Visitor&& visitor)
    {
        if (pattern.find_first_of("*?[{") == std::string_view::npos && pattern.find("//") == std::string_view::npos
            && !pattern.empty() && pattern.back() != '/')
        {
            const auto it = m_index.find(pattern);
            if (it == m_index.end())
                return { 0, false };
            visitor(m_methods[it->second].address, m_methods[it->second].value);
            return { 1, false };
        }
        if (const auto hit = m_cache.find(pattern); hit != m_cache.end())
        {
            for (const MethodId id : hit->second)
                visitor(m_methods[id].address, m_methods[id].value);
            return { hit->second.size(), false };
        }
        const Pattern p(pattern);
        if (!p.valid())
            return { 0, true };
        std::vector<MethodId> ids;
        for (MethodId id = 0; id < m_methods.size(); ++id)
            if (p.matches(m_methods[id].address))
            {
                visitor(m_methods[id].address, m_methods[id].value);
                ids.push_back(id);
            }
        if (m_cache.size() >= kCacheCap)
            m_cache.clear();
        const std::size_t n = ids.size();
        m_cache.emplace(std::string(pattern), std::move(ids));
        return { n, false };
    }

private:
    static constexpr std::size_t kCacheCap = 4096;
    struct Method
    {
        std::string address;
        T value;
    };
    std::vector<Method> m_methods;
    std::unordered_map<std::string, MethodId, detail::TransparentHash, std::equal_to<>> m_index;
    std::unordered_map<std::string, std::vector<MethodId>, detail::TransparentHash, std::equal_to<>> m_cache;
};

}
