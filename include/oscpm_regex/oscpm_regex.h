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

struct StringViewHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view> { }(text); }
};

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

/// Represents an OSC address pattern as a regular expression.
class Pattern
{
public:
    /// Translates and compiles `text`
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

    /// @returns whether the pattern compiled.
    bool valid() const { return m_valid; }

    /// @returns whether this pattern matches `address`.
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

/// OSC address pattern matcher
///
/// Implements simple caching which greatly speeds up repeat requests for the same match
class Matcher
{
public:
    static constexpr std::size_t kDefaultMaxPairs = 65536;

    explicit Matcher(std::size_t maxPairs = kDefaultMaxPairs)
        : m_maxPairs(maxPairs)
    {
    }

    /// @returns whether `pattern` matches `address`, as `match`.
    bool match(std::string_view pattern, std::string_view address)
    {
        if (const auto verdict = m_verdicts.find(detail::PairView { pattern, address }); verdict != m_verdicts.end())
            return verdict->second;
        const bool matched = oscpm_regex::match(pattern, address);
        if (m_verdicts.size() >= m_maxPairs)
            m_verdicts.clear();
        m_verdicts.emplace(detail::PairKey { std::string(pattern), std::string(address) }, matched);
        return matched;
    }

private:
    std::unordered_map<detail::PairKey, bool, detail::PairHash, detail::PairEqual> m_verdicts;
    std::size_t m_maxPairs;
};

/// A dispatch table of OSC methods keyed by address.
template <typename T>
class AddressSpace
{
public:
    /// Registers `value` under `address`
    /// @returns false if the address is already registered.
    bool add(std::string_view address, T value) { return m_methods.emplace(std::string(address), std::move(value)).second; }

    /// Unregisters `address`
    /// @returns false if it is not registered.
    bool remove(std::string_view address) { return m_methods.erase(std::string(address)) != 0; }

    /// @returns the number of registered methods.
    std::size_t size() const { return m_methods.size(); }

    /// Calls `callback(std::string_view address, T& value)` for every method
    /// `pattern` matches
    template <typename Callback>
        requires std::invocable<Callback, std::string_view, T&>
    std::size_t dispatch(std::string_view pattern, Callback&& callback)
    {
        if (const auto method = m_methods.find(pattern); method != m_methods.end())
        {
            callback(std::string_view(method->first), method->second);
            return 1;
        }
        std::size_t matched = 0;
        for (auto& [address, value] : m_methods)
        {
            if (m_matcher.match(pattern, address))
            {
                callback(std::string_view(address), value);
                ++matched;
            }
        }
        return matched;
    }

private:
    std::unordered_map<std::string, T, detail::StringViewHash, std::equal_to<>> m_methods;
    Matcher m_matcher;
};

}
