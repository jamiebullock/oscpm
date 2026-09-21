/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oscpm_regex::detail
{

inline std::string translatePart(std::string_view part)
{
    std::string expression;
    for (std::size_t i = 0; i < part.size(); ++i)
    {
        const char byte = part[i];
        if (byte == '?')
            expression += "[^/]";
        else if (byte == '*')
            expression += "[^/]*";
        else if (byte == '{')
            expression += "(?:";
        else if (byte == ',')
            expression += '|';
        else if (byte == '}')
            expression += ')';
        else if (byte == '!' && i > 0 && part[i - 1] == '[')
            expression += '^';
        else if (std::string_view(".^$+()|\\").find(byte) != std::string_view::npos)
            expression += std::string { '\\', byte };
        else
            expression += byte;
    }
    return expression;
}

inline std::string translate(std::string_view pattern)
{
    std::string expression;
    std::size_t start = 1;
    while (start <= pattern.size())
    {
        const std::size_t separator = std::min(pattern.find('/', start), pattern.size());
        const std::string_view part = pattern.substr(start, separator - start);
        expression += part.empty() ? "(?:/[^/]*)*" : "/" + translatePart(part);
        start = separator + 1;
    }
    return expression;
}

struct PairKey
{
    std::string pattern;
    std::string address;
};

struct PairView
{
    std::string_view pattern;
    std::string_view address;
};

struct PairHash
{
    using is_transparent = void;

    std::size_t operator()(PairView pair) const noexcept
    {
        return std::hash<std::string_view> { }(pair.pattern) * 31 + std::hash<std::string_view> { }(pair.address);
    }

    std::size_t operator()(const PairKey& pair) const noexcept { return (*this)(PairView { pair.pattern, pair.address }); }
};

struct PairEqual
{
    using is_transparent = void;

    bool operator()(const PairKey& left, PairView right) const noexcept { return left.pattern == right.pattern && left.address == right.address; }

    bool operator()(PairView left, const PairKey& right) const noexcept { return (*this)(right, left); }

    bool operator()(const PairKey& left, const PairKey& right) const noexcept { return (*this)(left, PairView { right.pattern, right.address }); }
};

}

namespace oscpm_regex
{

/// An OSC address pattern as a regular expression. Construction allocates,
/// and so does every call to `matches`.
class Pattern
{
public:
    /// Translates and compiles `text`; a pattern the regex engine rejects
    /// is invalid and matches nothing.
    explicit Pattern(std::string_view text)
    {
        try
        {
            m_expression = std::regex(detail::translate(text));
            m_valid = true;
        }
        catch (const std::regex_error&)
        {
        }
    }

    /// Whether the pattern compiled.
    bool valid() const { return m_valid; }

    /// Whether this pattern matches `address`.
    bool matches(std::string_view address) const
    {
        return m_valid && !address.empty() && address[0] == '/' && std::regex_match(address.begin(), address.end(), m_expression);
    }

private:
    std::regex m_expression;
    bool m_valid = false;
};

/// Compiles `pattern` and tests it against `address`.
inline bool match(std::string_view pattern, std::string_view address)
{
    return Pattern(pattern).matches(address);
}

/// Memoises match verdicts by pattern and address pair. The first call for
/// a pair compiles and matches, which allocates; every later call is a hash
/// lookup that allocates nothing. Not safe for concurrent use.
class Matcher
{
public:
    /// Whether `pattern` matches `address`, as `match`.
    bool match(std::string_view pattern, std::string_view address)
    {
        if (const auto verdict = m_verdicts.find(detail::PairView { pattern, address }); verdict != m_verdicts.end())
            return verdict->second;
        const bool matched = oscpm_regex::match(pattern, address);
        m_verdicts.emplace(detail::PairKey { std::string(pattern), std::string(address) }, matched);
        return matched;
    }

    /// How many pairs are memoised.
    std::size_t size() const { return m_verdicts.size(); }

    /// Forgets every memoised verdict.
    void clear() { m_verdicts.clear(); }

private:
    std::unordered_map<detail::PairKey, bool, detail::PairHash, detail::PairEqual> m_verdicts;
};

/// A set of methods, each an address with a value of type `T`, that a
/// pattern is dispatched to by asking a `Matcher` about every method in
/// insertion order. `add` and `remove` allocate. Not safe for concurrent
/// use.
template <typename T>
class AddressSpace
{
public:
    /// Registers `value` under `address`; false if the address is already
    /// registered.
    bool add(std::string_view address, T value)
    {
        if (find(address) != m_methods.end())
            return false;
        m_methods.push_back(Method { std::string(address), std::move(value) });
        return true;
    }

    /// Unregisters `address`; false if it is not registered.
    bool remove(std::string_view address)
    {
        const auto position = find(address);
        if (position == m_methods.end())
            return false;
        m_methods.erase(position);
        return true;
    }

    /// The number of registered methods.
    std::size_t size() const { return m_methods.size(); }

    /// The matcher whose memo serves every dispatch.
    Matcher& matcher() { return m_matcher; }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches and returns how many. The visitor must not add or
    /// remove methods.
    template <typename Visitor>
    std::size_t dispatch(std::string_view pattern, Visitor&& visitor)
    {
        std::size_t matched = 0;
        for (Method& method : m_methods)
        {
            if (m_matcher.match(pattern, method.address))
            {
                visitor(std::string_view(method.address), method.value);
                ++matched;
            }
        }
        return matched;
    }

private:
    struct Method
    {
        std::string address;
        T value;
    };

    typename std::vector<Method>::iterator find(std::string_view address)
    {
        return std::find_if(m_methods.begin(), m_methods.end(), [address](const Method& method)
            { return method.address == address; });
    }

    std::vector<Method> m_methods;
    Matcher m_matcher;
};

}
