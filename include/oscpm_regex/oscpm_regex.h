/* Part of oscpm-regex
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oscpm_regex
{

/// A fault in a pattern, in an address or in an `AddressSpace` operation.
/// `MissingLeadingSlash` and `IllegalByte` are reported for both a pattern
/// and an address; `UnterminatedClass`, `UnterminatedBraces`,
/// `NestedBraces` and `RegexRejected`, a translation the regex engine
/// refused, only for a pattern; `TrailingSlash` and `EmptyPart` only for an
/// address; `Duplicate` and `NotFound` by `add` and `remove`.
enum class Error : std::uint8_t
{
    MissingLeadingSlash,
    IllegalByte,
    UnterminatedClass,
    UnterminatedBraces,
    NestedBraces,
    RegexRejected,
    TrailingSlash,
    EmptyPart,
    Duplicate,
    NotFound
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
    constexpr std::string_view descendantOperator = "//";
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
    constexpr std::size_t hashMixer = 0x9E3779B9;
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

inline bool isLiteral(std::string_view pattern)
{
    return !pattern.empty() && pattern.find_first_of(k::operatorBytes) == npos && pattern.find(k::descendantOperator) == npos && pattern.back() != k::partSeparator;
}

inline void appendEscapedByte(std::string& expression, char byte)
{
    const auto value = static_cast<unsigned char>(byte);
    expression += "\\x";
    expression += k::hexDigits[value >> 4];
    expression += k::hexDigits[value & 0x0F];
}

class ByteSet
{
public:
    bool contains(unsigned char value) const { return m_members[value]; }

    void insert(unsigned char value) { m_members[value] = true; }

    void insertRange(unsigned char first, unsigned char last)
    {
        for (unsigned value = first; value <= last; ++value)
            m_members[value] = true;
    }

    void erase(unsigned char value) { m_members[value] = false; }

    void complement()
    {
        for (bool& member : m_members)
            member = !member;
    }

private:
    std::array<bool, k::numByteValues> m_members { };
};

inline ByteSet classMembers(std::string_view body)
{
    ByteSet members;
    const bool negate = !body.empty() && body[0] == k::setNegate;
    std::size_t i = negate ? 1 : 0;
    while (i < body.size())
    {
        const auto first = static_cast<unsigned char>(body[i]);
        if (i + 2 < body.size() && body[i + 1] == k::rangeSeparator)
        {
            members.insertRange(first, static_cast<unsigned char>(body[i + 2]));
            i += 3;
        }
        else
        {
            members.insert(first);
            ++i;
        }
    }
    if (negate)
    {
        members.complement();
        members.erase(static_cast<unsigned char>(k::partSeparator));
    }
    return members;
}

inline void appendByteClass(std::string& expression, const ByteSet& members)
{
    expression += k::setOpen;
    bool hasMembers = false;
    for (unsigned value = 0; value < k::numByteValues;)
    {
        if (!members.contains(static_cast<unsigned char>(value)))
        {
            ++value;
            continue;
        }
        unsigned rangeEnd = value;
        while (rangeEnd + 1 < k::numByteValues && members.contains(static_cast<unsigned char>(rangeEnd + 1)))
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

inline std::optional<Error> appendPart(std::string& expression, std::string_view part)
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
            appendByteClass(expression, classMembers(part.substr(i + 1, close - i - 1)));
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
    return std::nullopt;
}

struct Translation
{
    std::optional<Error> error;
    std::string expression;
};

inline Translation translatePattern(std::string_view pattern)
{
    if (pattern.empty() || pattern[0] != k::partSeparator)
        return { Error::MissingLeadingSlash, { } };
    for (const char byte : pattern)
    {
        if (!isPrintableAscii(byte))
            return { Error::IllegalByte, { } };
    }
    Translation translation;
    std::size_t i = 1;
    bool insideDescendantOperator = false;
    for (;;)
    {
        const std::size_t separator = pattern.find(k::partSeparator, i);
        const std::string_view part = pattern.substr(i, separator == npos ? npos : separator - i);
        if (part.empty())
        {
            if (!insideDescendantOperator)
                translation.expression += k::descendantExpression;
            insideDescendantOperator = true;
        }
        else
        {
            translation.expression += k::partSeparator;
            if (const std::optional<Error> error = appendPart(translation.expression, part))
                return { error, { } };
            insideDescendantOperator = false;
        }
        if (separator == npos)
            break;
        i = separator + 1;
    }
    return translation;
}

struct StringViewHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view> { }(text); }
    std::size_t operator()(const std::string& text) const noexcept { return std::hash<std::string_view> { }(text); }
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
        const std::size_t patternHash = std::hash<std::string_view> { }(pair.pattern);
        const std::size_t addressHash = std::hash<std::string_view> { }(pair.address);
        return patternHash ^ (addressHash + k::hashMixer + (patternHash << 6) + (patternHash >> 2));
    }

    std::size_t operator()(const PairKey& pair) const noexcept { return (*this)(PairView { pair.pattern, pair.address }); }
};

struct PairEqual
{
    using is_transparent = void;

    bool operator()(const PairKey& left, const PairKey& right) const noexcept { return left.pattern == right.pattern && left.address == right.address; }

    bool operator()(const PairKey& left, PairView right) const noexcept { return left.pattern == right.pattern && left.address == right.address; }

    bool operator()(PairView left, const PairKey& right) const noexcept { return left.pattern == right.pattern && left.address == right.address; }
};

}

namespace oscpm_regex
{

/// The first fault in `address`, or nothing when it is a well-formed OSC
/// address: `MissingLeadingSlash`, `TrailingSlash` (the bare "/" included),
/// `EmptyPart` for two adjacent slashes, `IllegalByte` for a byte outside
/// printable ASCII or one of `#*,?[]{}`.
inline std::optional<Error> validateAddress(std::string_view address)
{
    if (address.empty() || address[0] != detail::k::partSeparator)
        return Error::MissingLeadingSlash;
    if (address.back() == detail::k::partSeparator)
        return Error::TrailingSlash;
    for (std::size_t i = 1; i < address.size(); ++i)
    {
        if (address[i] == detail::k::partSeparator && address[i - 1] == detail::k::partSeparator)
            return Error::EmptyPart;
        if (!detail::isPrintableAscii(address[i]) || detail::isReservedInAddress(address[i]))
            return Error::IllegalByte;
    }
    return std::nullopt;
}

/// An OSC address pattern compiled to a regular expression. Construction
/// allocates, and so does every call to `matches`.
class Pattern
{
public:
    /// An invalid pattern that matches nothing.
    Pattern() = default;

    /// Compiles `text`; `error` reports why that failed.
    explicit Pattern(std::string_view text)
    {
        const detail::Translation translation = detail::translatePattern(text);
        m_error = translation.error;
        if (m_error)
            return;
        try
        {
            m_expression = std::regex(translation.expression, std::regex::ECMAScript);
        }
        catch (const std::regex_error&)
        {
            m_error = Error::RegexRejected;
        }
    }

    /// Whether the pattern compiled.
    bool valid() const { return !m_error.has_value(); }

    /// Why the pattern did not compile, or nothing when it did.
    std::optional<Error> error() const { return m_error; }

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
    std::optional<Error> m_error = Error::MissingLeadingSlash;
};

/// Compiles `pattern` and tests it against `address`.
inline bool match(std::string_view pattern, std::string_view address)
{
    return Pattern(pattern).matches(address);
}

/// Memoises match verdicts. The first call for a pattern compiles it and
/// the first call for a pattern and address pair matches it, both of which
/// allocate; every later call for the same pair is a hash lookup that
/// allocates nothing. A malformed pattern matches nothing and is memoised
/// as such. Either table is emptied when it reaches its limit. Not safe
/// for concurrent use.
class Matcher
{
public:
    /// Whether `pattern` matches `address`, as `Pattern::matches`.
    bool match(std::string_view pattern, std::string_view address)
    {
        if (const auto verdict = m_verdicts.find(detail::PairView { pattern, address }); verdict != m_verdicts.end())
            return verdict->second;
        const bool matched = compiled(pattern).matches(address);
        if (m_verdicts.size() >= kMaxCachedVerdicts)
            m_verdicts.clear();
        m_verdicts.emplace(detail::PairKey { std::string(pattern), std::string(address) }, matched);
        return matched;
    }

    /// The compiled form of `pattern`, kept for later calls.
    const Pattern& compiled(std::string_view pattern)
    {
        if (const auto known = m_patterns.find(pattern); known != m_patterns.end())
            return known->second;
        if (m_patterns.size() >= kMaxCachedPatterns)
            m_patterns.clear();
        return m_patterns.emplace(std::string(pattern), Pattern(pattern)).first->second;
    }

    /// How many pattern and address pairs are memoised.
    std::size_t size() const { return m_verdicts.size(); }

    /// Forgets every memoised verdict and compiled pattern.
    void clear()
    {
        m_verdicts.clear();
        m_patterns.clear();
    }

    static constexpr std::size_t kMaxCachedPatterns = 4096;
    static constexpr std::size_t kMaxCachedVerdicts = 65536;

private:
    std::unordered_map<std::string, Pattern, detail::StringViewHash, std::equal_to<>> m_patterns;
    std::unordered_map<detail::PairKey, bool, detail::PairHash, detail::PairEqual> m_verdicts;
};

/// A set of methods, each a well-formed address with a value of type `T`,
/// that a pattern is dispatched to. Addresses are kept in bytewise order
/// and visited in that order. A dispatch asks a `Matcher` about every
/// method in turn, so it costs one memoised match per method: a hash
/// lookup each once the pattern has been seen against every address, and
/// a regex match each before that. `add` and `remove` allocate. Not safe
/// for concurrent use.
template <typename T>
class AddressSpace
{
public:
    /// How many methods a dispatch visited, and the fault that stopped the
    /// pattern compiling, in which case it visited none.
    struct DispatchResult
    {
        std::size_t matched;
        std::optional<Error> error;
    };

    /// Registers `value` under `address`. Fails with the `validateAddress`
    /// fault, or `Duplicate` when the address is already registered.
    std::optional<Error> add(std::string_view address, T value)
    {
        if (const std::optional<Error> fault = validateAddress(address))
            return fault;
        const auto position = lowerBound(address);
        if (position != m_methods.end() && position->address == address)
            return Error::Duplicate;
        m_methods.insert(position, Method { std::string(address), std::move(value) });
        return std::nullopt;
    }

    /// Unregisters `address`. Fails with the `validateAddress` fault, or
    /// `NotFound` when the address is not registered.
    std::optional<Error> remove(std::string_view address)
    {
        if (const std::optional<Error> fault = validateAddress(address))
            return fault;
        const auto position = lowerBound(address);
        if (position == m_methods.end() || position->address != address)
            return Error::NotFound;
        m_methods.erase(position);
        return std::nullopt;
    }

    /// The number of registered methods.
    std::size_t size() const { return m_methods.size(); }

    /// The matcher whose memo serves every dispatch.
    Matcher& matcher() { return m_matcher; }

    /// Calls `visitor(std::string_view address, T& value)` for every method
    /// `pattern` matches, in bytewise address order. The visitor must not add
    /// or remove methods.
    template <typename Visitor>
    DispatchResult dispatch(std::string_view pattern, Visitor&& visitor)
    {
        const Pattern& compiled = m_matcher.compiled(pattern);
        if (!compiled.valid())
            return { 0, compiled.error() };
        std::size_t matched = 0;
        for (Method& method : m_methods)
        {
            if (m_matcher.match(pattern, method.address))
            {
                visitor(std::string_view(method.address), method.value);
                ++matched;
            }
        }
        return { matched, std::nullopt };
    }

private:
    struct Method
    {
        std::string address;
        T value;
    };

    typename std::vector<Method>::iterator lowerBound(std::string_view address)
    {
        return std::lower_bound(m_methods.begin(), m_methods.end(), address, [](const Method& method, std::string_view candidate)
            { return method.address < candidate; });
    }

    std::vector<Method> m_methods;
    Matcher m_matcher;
};

}
