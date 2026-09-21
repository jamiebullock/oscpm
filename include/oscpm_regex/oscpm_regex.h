/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <cstdint>
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

namespace k
{
    constexpr char partSeparator = '/';
    constexpr char anyByte = '?';
    constexpr char anyBytes = '*';
    constexpr char setOpen = '[';
    constexpr char setClose = ']';
    constexpr char setNegate = '!';
    constexpr char rangeSeparator = '-';
    constexpr char listOpen = '{';
    constexpr char listClose = '}';
    constexpr char listSeparator = ',';
    constexpr std::string_view operatorBytes = "*?[{";
    constexpr std::string_view reservedInAddress = "#*,?[]{}";
    constexpr unsigned char firstPrintableAscii = 0x21;
    constexpr unsigned char lastPrintableAscii = 0x7E;
    constexpr std::size_t numByteValues = 256;
    constexpr std::string_view hexDigits = "0123456789ABCDEF";
    constexpr std::string_view anyByteExpression = "[^/]";
    constexpr std::string_view anyBytesExpression = "[^/]*";
    constexpr std::string_view descendantExpression = "(?:/[^/]*)*";
    constexpr std::string_view noByteExpression = "^\\x00-\\xFF";
    constexpr std::string_view alternativesOpen = "(?:";
    constexpr char alternativesSeparator = '|';
    constexpr char alternativesClose = ')';
}

constexpr std::size_t npos = std::string_view::npos;

inline bool isPrintableAscii(char byte)
{
    const auto value = static_cast<unsigned char>(byte);
    return value >= k::firstPrintableAscii && value <= k::lastPrintableAscii;
}

inline bool isReservedInAddress(char byte)
{
    return k::reservedInAddress.find(byte) != npos;
}

inline void appendEscapedByte(std::string& expression, char byte)
{
    const auto value = static_cast<unsigned char>(byte);
    expression += "\\x";
    expression += k::hexDigits[value >> 4];
    expression += k::hexDigits[value & 0x0F];
}

inline void classMembers(std::string_view body, bool (&members)[k::numByteValues])
{
    std::memset(members, 0, sizeof members);
    bool negate = false;
    std::size_t i = 0;
    if (!body.empty() && body[0] == k::setNegate)
    {
        negate = true;
        i = 1;
    }
    while (i < body.size())
    {
        const auto first = static_cast<unsigned char>(body[i]);
        if (i + 2 < body.size() && body[i + 1] == k::rangeSeparator)
        {
            const auto last = static_cast<unsigned char>(body[i + 2]);
            for (unsigned value = first; value <= last; ++value)
                members[value] = true;
            i += 3;
        }
        else
        {
            members[first] = true;
            ++i;
        }
    }
    if (negate)
    {
        for (bool& member : members)
            member = !member;
        members[static_cast<unsigned char>(k::partSeparator)] = false;
    }
}

inline void appendByteClass(std::string& expression, const bool (&members)[k::numByteValues])
{
    expression += k::setOpen;
    bool hasMembers = false;
    for (unsigned value = 0; value < k::numByteValues;)
    {
        if (!members[value])
        {
            ++value;
            continue;
        }
        unsigned rangeEnd = value;
        while (rangeEnd + 1 < k::numByteValues && members[rangeEnd + 1])
            ++rangeEnd;
        appendEscapedByte(expression, static_cast<char>(value));
        if (rangeEnd > value)
        {
            expression += k::rangeSeparator;
            appendEscapedByte(expression, static_cast<char>(rangeEnd));
        }
        hasMembers = true;
        value = rangeEnd + 1;
    }
    if (!hasMembers)
        expression += k::noByteExpression;
    expression += k::setClose;
}

inline Error appendPart(std::string& expression, std::string_view part)
{
    std::size_t i = 0;
    while (i < part.size())
    {
        const char byte = part[i];
        if (byte == k::anyByte)
        {
            expression += k::anyByteExpression;
            ++i;
        }
        else if (byte == k::anyBytes)
        {
            expression += k::anyBytesExpression;
            ++i;
        }
        else if (byte == k::setOpen)
        {
            const bool negate = i + 1 < part.size() && part[i + 1] == k::setNegate;
            const std::size_t close = part.find(k::setClose, i + 1 + (negate ? 1 : 0));
            if (close == npos)
                return Error::UnterminatedClass;
            bool members[k::numByteValues];
            classMembers(part.substr(i + 1, close - i - 1), members);
            appendByteClass(expression, members);
            i = close + 1;
        }
        else if (byte == k::listOpen)
        {
            const std::size_t close = part.find(k::listClose, i + 1);
            if (close == npos)
                return Error::UnterminatedBraces;
            const std::string_view body = part.substr(i + 1, close - i - 1);
            if (body.find(k::listOpen) != npos)
                return Error::NestedBraces;
            expression += k::alternativesOpen;
            for (const char member : body)
            {
                if (member == k::listSeparator)
                    expression += k::alternativesSeparator;
                else
                    appendEscapedByte(expression, member);
            }
            expression += k::alternativesClose;
            i = close + 1;
        }
        else
        {
            appendEscapedByte(expression, byte);
            ++i;
        }
    }
    return Error::None;
}

inline Error translatePattern(std::string_view pattern, std::string& expression)
{
    expression.clear();
    if (pattern.empty() || pattern[0] != k::partSeparator)
        return Error::MissingLeadingSlash;
    for (const char byte : pattern)
    {
        if (!isPrintableAscii(byte))
            return Error::IllegalByte;
    }
    std::size_t i = 1;
    bool insideDescendantOperator = false;
    for (;;)
    {
        const std::size_t separator = pattern.find(k::partSeparator, i);
        const std::string_view part = pattern.substr(i, separator == npos ? npos : separator - i);
        if (part.empty())
        {
            if (!insideDescendantOperator)
                expression += k::descendantExpression;
            insideDescendantOperator = true;
        }
        else
        {
            expression += k::partSeparator;
            if (const Error error = appendPart(expression, part); error != Error::None)
                return error;
            insideDescendantOperator = false;
        }
        if (separator == npos)
            break;
        i = separator + 1;
    }
    return Error::None;
}

struct StringViewHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view> { }(text); }
    std::size_t operator()(const std::string& text) const noexcept { return std::hash<std::string_view> { }(text); }
};

}

namespace oscpm_regex
{

/// Whether `address` is a well-formed OSC address: a leading '/', no
/// trailing or doubled '/', and only printable ASCII other than `#*,?[]{}`.
inline bool isValidAddress(std::string_view address)
{
    if (address.empty() || address[0] != detail::k::partSeparator || address.back() == detail::k::partSeparator)
        return false;
    for (std::size_t i = 1; i < address.size(); ++i)
    {
        if (!detail::isPrintableAscii(address[i]) || detail::isReservedInAddress(address[i]))
            return false;
        if (address[i] == detail::k::partSeparator && address[i - 1] == detail::k::partSeparator)
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
        std::string expression;
        m_error = detail::translatePattern(text, expression);
        if (m_error != Error::None)
            return;
        try
        {
            m_expression = std::regex(expression, std::regex::ECMAScript);
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
        if (!valid() || address.empty() || address[0] != detail::k::partSeparator)
            return false;
        try
        {
            return std::regex_match(address.data(), address.data() + address.size(), m_expression);
        }
        catch (const std::regex_error&)
        {
            return false; // error_complexity or error_stack
        }
    }

private:
    std::regex m_expression;
    Error m_error = Error::MissingLeadingSlash;
};

/// Compiles `pattern` and tests it against `address`.
inline bool match(std::string_view pattern, std::string_view address)
{
    return Pattern(pattern).matches(address);
}

/// A set of methods, each a well-formed address with a value of type `T`,
/// that a pattern is dispatched to. An exact address and a pattern seen
/// since the last `add` or `remove` are served from hash maps without
/// allocating; any other pattern is compiled and matched against every
/// method, which allocates. Not safe for concurrent use.
template <typename T>
class Registry
{
public:
    using MethodIndex = std::uint32_t;

    /// How many methods a dispatch visited, and whether the pattern was
    /// malformed, in which case it visited none.
    struct DispatchResult
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
        m_index.emplace(m_methods.back().address, static_cast<MethodIndex>(m_methods.size() - 1));
        m_cache.clear();
        return true;
    }

    /// Unregisters `address`; false if it is not registered.
    bool remove(std::string_view address)
    {
        const auto method = m_index.find(address);
        if (method == m_index.end())
            return false;
        const MethodIndex index = method->second, last = static_cast<MethodIndex>(m_methods.size() - 1);
        m_index.erase(method);
        if (index != last)
        {
            m_methods[index] = std::move(m_methods[last]);
            m_index.find(m_methods[index].address)->second = index;
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
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        if (pattern.find_first_of(detail::k::operatorBytes) == detail::npos && pattern.find("//") == detail::npos && !pattern.empty() && pattern.back() != detail::k::partSeparator)
        {
            const auto method = m_index.find(pattern);
            if (method == m_index.end())
                return { 0, false };
            visitor(m_methods[method->second].address, m_methods[method->second].value);
            return { 1, false };
        }
        if (const auto cached = m_cache.find(pattern); cached != m_cache.end())
        {
            for (const MethodIndex index : cached->second)
                visitor(m_methods[index].address, m_methods[index].value);
            return { cached->second.size(), false };
        }
        const Pattern compiled(pattern);
        if (!compiled.valid())
            return { 0, true };
        std::vector<MethodIndex> matched;
        for (MethodIndex index = 0; index < m_methods.size(); ++index)
        {
            if (compiled.matches(m_methods[index].address))
            {
                visitor(m_methods[index].address, m_methods[index].value);
                matched.push_back(index);
            }
        }
        if (m_cache.size() >= kMaxCachedPatterns)
            m_cache.clear();
        const std::size_t numMatched = matched.size();
        m_cache.emplace(std::string(pattern), std::move(matched));
        return { numMatched, false };
    }

private:
    static constexpr std::size_t kMaxCachedPatterns = 4096;

    struct Method
    {
        std::string address;
        T value;
    };

    std::vector<Method> m_methods;
    std::unordered_map<std::string, MethodIndex, detail::StringViewHash, std::equal_to<>> m_index;
    std::unordered_map<std::string, std::vector<MethodIndex>, detail::StringViewHash, std::equal_to<>> m_cache;
};

}
