/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oscpm::detail
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

}

namespace oscpm
{

/// Represents an OSC address pattern as a regular expression.
class Pattern
{
public:
    /// Translates and compiles `text` to a regex ignoring malformed patterns
    explicit Pattern(std::string_view text)
    {
        if (text.size() < 2 || text[0] != '/')
            return;
        try
        {
            m_expression = std::regex(detail::translate(text));
            m_valid = true;
        }
        catch (const std::regex_error&)
        {
        }
    }

    /// @returns whether the pattern is valid. see README for spec
    bool valid() const { return m_valid; }

    /// @returns whether this pattern matches `address`.
    bool matches(std::string_view address) const
    {
        if (!m_valid || address.empty() || address[0] != '/')
            return false;
        try
        {
            return std::regex_match(address.begin(), address.end(), m_expression);
        }
        catch (const std::regex_error&)
        {
            return false;
        }
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

/// A dispatch table of OSC methods keyed by address.
template <typename T>
class AddressSpace
{
public:
    /// Registers `value` under `address`
    /// @returns false if the address is already registered.
    bool add(std::string_view address, T value)
    {
        m_reached.clear();
        return m_methods.emplace(std::string(address), std::move(value)).second;
    }

    /// Unregisters `address`
    /// @returns false if it is not registered.
    bool remove(std::string_view address)
    {
        m_reached.clear();
        return m_methods.erase(std::string(address)) != 0;
    }

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
        auto reached = m_reached.find(pattern);
        if (reached == m_reached.end())
        {
            const Pattern compiled(pattern);
            std::vector<Method*> methods;
            for (Method& method : m_methods)
            {
                if (compiled.matches(method.first))
                    methods.push_back(&method);
            }
            if (m_reached.size() >= kMaxPatterns)
                m_reached.clear();
            reached = m_reached.emplace(std::string(pattern), std::move(methods)).first;
        }
        for (Method* method : reached->second)
            callback(std::string_view(method->first), method->second);
        return reached->second.size();
    }

private:
    using Method = std::pair<const std::string, T>;
    static constexpr std::size_t kMaxPatterns = 4096;

    std::unordered_map<std::string, T, detail::StringViewHash, std::equal_to<>> m_methods;
    std::unordered_map<std::string, std::vector<Method*>, detail::StringViewHash, std::equal_to<>> m_reached;
};

}
