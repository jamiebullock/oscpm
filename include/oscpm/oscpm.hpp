// oscpm - OSC address pattern matching for C++17
//
// Copyright (c) 2026 Jamie Bullock
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef OSCPM_OSCPM_HPP_INCLUDED
#define OSCPM_OSCPM_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Pattern syntax (OSC 1.0 plus the OSC 1.1 descendant operator):
//
//   ?          any single character except '/'
//   *          any run of zero or more characters, none of them '/'
//   [abc]      any one of the listed characters
//   [a-z]      any character in the ASCII range
//   [!abc]     any character not listed
//   {foo,bar}  any one of the comma-separated literal alternatives
//   //         zero or more whole address parts (OSC 1.1)
//
// Every other printable ASCII character matches itself. A pattern and an
// address must both begin with '/'. Malformed patterns are rejected rather
// than interpreted; validate() reports why, and a rejected pattern never
// matches anything.
//
// The matching path allocates no memory and throws no exceptions.

namespace oscpm {

/// Why a pattern was rejected. Error::None means the pattern is well formed.
enum class Error : std::uint8_t {
    None = 0,
    Empty,                      ///< The pattern has no characters.
    MissingLeadingSlash,        ///< The pattern does not begin with '/'.
    EmptyPart,                  ///< A '/' is followed by nothing or by a third '/'.
    UnterminatedCharacterClass, ///< A '[' has no closing ']'.
    EmptyCharacterClass,        ///< "[]" or "[!]".
    InvalidRange,               ///< A range such as "[z-a]" runs backwards.
    UnterminatedAlternatives,   ///< A '{' has no closing '}'.
    EmptyAlternatives,          ///< "{}".
    UnexpectedCharacter,        ///< A character that is not allowed where it appears.
};

/// A short human-readable description of an Error.
constexpr const char* errorMessage(Error error) noexcept
{
    switch (error) {
    case Error::None: return "no error";
    case Error::Empty: return "pattern is empty";
    case Error::MissingLeadingSlash: return "pattern must begin with '/'";
    case Error::EmptyPart: return "address part is empty";
    case Error::UnterminatedCharacterClass: return "'[' without closing ']'";
    case Error::EmptyCharacterClass: return "character class is empty";
    case Error::InvalidRange: return "character range runs backwards";
    case Error::UnterminatedAlternatives: return "'{' without closing '}'";
    case Error::EmptyAlternatives: return "alternatives are empty";
    case Error::UnexpectedCharacter: return "unexpected character";
    }
    return "unknown error";
}

namespace detail {

constexpr bool isPrintableAscii(char c) noexcept
{
    const auto u = static_cast<unsigned char>(c);
    return u >= 0x20 && u <= 0x7E;
}

constexpr bool isRange(std::string_view p, std::size_t i, std::size_t end) noexcept
{
    // p[i] is a class member; a following "-x" forms a range unless the '-'
    // is the last character before the closing ']' at index `end`.
    return i + 2 < end && p[i + 1] == '-';
}

constexpr bool isClassMember(char c) noexcept
{
    return isPrintableAscii(c) && c != '/' && c != '[';
}

/// Validate the character class beginning at p[i] == '['. On success `end`
/// receives the index of the closing ']'.
constexpr Error validateClass(std::string_view p, std::size_t i, std::size_t& end) noexcept
{
    std::size_t j = i + 1;
    if (j < p.size() && p[j] == '!') ++j;
    if (j >= p.size()) return Error::UnterminatedCharacterClass;
    if (p[j] == ']') return Error::EmptyCharacterClass;

    const std::size_t close = p.find(']', j);
    if (close == std::string_view::npos) return Error::UnterminatedCharacterClass;

    while (j < close) {
        const char lo = p[j];
        if (!isClassMember(lo)) return Error::UnexpectedCharacter;
        if (isRange(p, j, close)) {
            const char hi = p[j + 2];
            if (!isClassMember(hi)) return Error::UnexpectedCharacter;
            if (static_cast<unsigned char>(hi) < static_cast<unsigned char>(lo)) return Error::InvalidRange;
            j += 3;
        } else {
            ++j;
        }
    }
    end = close;
    return Error::None;
}

constexpr bool isAlternativeMember(char c) noexcept
{
    return isPrintableAscii(c) && c != '/' && c != '{' && c != '}' && c != '[' && c != ']' && c != '*' && c != '?';
}

/// Validate the alternatives beginning at p[i] == '{'. On success `end`
/// receives the index of the closing '}'.
constexpr Error validateAlternatives(std::string_view p, std::size_t i, std::size_t& end) noexcept
{
    std::size_t j = i + 1;
    if (j < p.size() && p[j] == '}') return Error::EmptyAlternatives;
    while (j < p.size() && p[j] != '}') {
        if (!isAlternativeMember(p[j])) return Error::UnexpectedCharacter;
        ++j;
    }
    if (j >= p.size()) return Error::UnterminatedAlternatives;
    end = j;
    return Error::None;
}

struct Info {
    Error error;
    bool literal; ///< True when the pattern contains no operators at all.
};

constexpr Info validate(std::string_view p) noexcept
{
    if (p.empty()) return {Error::Empty, false};
    if (p[0] != '/') return {Error::MissingLeadingSlash, false};

    bool literal = true;
    std::size_t i = 0;
    while (i < p.size()) {
        switch (p[i]) {
        case '/': {
            std::size_t next = i + 1;
            if (next < p.size() && p[next] == '/') {
                literal = false;
                ++next;
            }
            if (next >= p.size() || p[next] == '/') return {Error::EmptyPart, false};
            i = next;
            break;
        }
        case '*':
        case '?':
            literal = false;
            ++i;
            break;
        case '[': {
            std::size_t end = 0;
            const Error e = validateClass(p, i, end);
            if (e != Error::None) return {e, false};
            literal = false;
            i = end + 1;
            break;
        }
        case '{': {
            std::size_t end = 0;
            const Error e = validateAlternatives(p, i, end);
            if (e != Error::None) return {e, false};
            literal = false;
            i = end + 1;
            break;
        }
        case ']':
        case '}':
        case ',':
            return {Error::UnexpectedCharacter, false};
        default:
            if (!isPrintableAscii(p[i])) return {Error::UnexpectedCharacter, false};
            ++i;
            break;
        }
    }
    return {Error::None, literal};
}

/// Does character `c` belong to the class whose members are `cls`
/// (the text between '[' and ']')?
constexpr bool matchClass(std::string_view cls, char c) noexcept
{
    bool negate = false;
    std::size_t j = 0;
    if (!cls.empty() && cls[0] == '!') {
        negate = true;
        ++j;
    }
    const auto u = static_cast<unsigned char>(c);
    bool found = false;
    while (j < cls.size()) {
        const auto lo = static_cast<unsigned char>(cls[j]);
        if (isRange(cls, j, cls.size())) {
            const auto hi = static_cast<unsigned char>(cls[j + 2]);
            if (u >= lo && u <= hi) found = true;
            j += 3;
        } else {
            if (u == lo) found = true;
            ++j;
        }
    }
    return found != negate;
}

/// Match a validated pattern against an address. Both are consumed from the
/// left; the function recurses only at the backtracking points ('*', '{'
/// and '//'), so stack depth is bounded by the number of operators.
constexpr bool matchHere(std::string_view p, std::string_view a) noexcept
{
    while (!p.empty()) {
        const char c = p[0];
        switch (c) {
        case '*': {
            std::size_t k = 1;
            while (k < p.size() && p[k] == '*') ++k;
            p.remove_prefix(k);
            if (p.empty()) return a.find('/') == std::string_view::npos;
            for (std::size_t i = 0;; ++i) {
                if (matchHere(p, a.substr(i))) return true;
                if (i >= a.size() || a[i] == '/') return false;
            }
        }
        case '?':
            if (a.empty() || a[0] == '/') return false;
            p.remove_prefix(1);
            a.remove_prefix(1);
            break;
        case '[': {
            const std::size_t end = p.find(']', 1);
            if (a.empty() || a[0] == '/') return false;
            if (!matchClass(p.substr(1, end - 1), a[0])) return false;
            p.remove_prefix(end + 1);
            a.remove_prefix(1);
            break;
        }
        case '{': {
            const std::size_t end = p.find('}', 1);
            std::string_view alts = p.substr(1, end - 1);
            const std::string_view rest = p.substr(end + 1);
            for (;;) {
                const std::size_t comma = alts.find(',');
                const std::string_view alt = alts.substr(0, comma);
                if (a.substr(0, alt.size()) == alt && matchHere(rest, a.substr(alt.size()))) return true;
                if (comma == std::string_view::npos) return false;
                alts.remove_prefix(comma + 1);
            }
        }
        case '/':
            if (p.size() > 1 && p[1] == '/') {
                // The operator stands in for a part separator, so the address
                // must be at one; from there, skip zero or more whole parts.
                if (a.empty() || a[0] != '/') return false;
                p.remove_prefix(2);
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (a[i] == '/' && matchHere(p, a.substr(i + 1))) return true;
                }
                return false;
            }
            [[fallthrough]];
        default:
            if (a.empty() || a[0] != c) return false;
            p.remove_prefix(1);
            a.remove_prefix(1);
            break;
        }
    }
    return a.empty();
}

constexpr bool isAddress(std::string_view address) noexcept
{
    return !address.empty() && address[0] == '/';
}

} // namespace detail

/// Check whether `pattern` is well formed. Returns Error::None if it is.
constexpr Error validate(std::string_view pattern) noexcept
{
    return detail::validate(pattern).error;
}

/// Match `pattern` against `address` in one go. The pattern is validated
/// first; a malformed pattern, or an address that does not begin with '/',
/// never matches.
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    if (validate(pattern) != Error::None) return false;
    if (!detail::isAddress(address)) return false;
    return detail::matchHere(pattern, address);
}

/// A pattern validated once and matched many times.
///
/// Construction copies and validates the pattern text, so it may allocate.
/// matches() does not allocate and does not throw. A Pattern that failed
/// validation reports why through error() and never matches anything.
class Pattern {
public:
    /// An empty, invalid pattern.
    Pattern() = default;

    explicit Pattern(std::string_view pattern)
        : str_(pattern)
    {
        const detail::Info info = detail::validate(str_);
        error_ = info.error;
        literal_ = info.literal;
    }

    /// Why the pattern was rejected, or Error::None.
    Error error() const noexcept { return error_; }

    bool valid() const noexcept { return error_ == Error::None; }
    explicit operator bool() const noexcept { return valid(); }

    /// True when the pattern contains no operators, so it matches exactly
    /// one address: its own text.
    bool isLiteral() const noexcept { return valid() && literal_; }

    /// The pattern text as given.
    std::string_view str() const noexcept { return str_; }

    bool matches(std::string_view address) const noexcept
    {
        if (!valid() || !detail::isAddress(address)) return false;
        if (literal_) return str_ == address;
        return detail::matchHere(str_, address);
    }

private:
    std::string str_;
    Error error_ = Error::Empty;
    bool literal_ = false;
};

} // namespace oscpm

#endif // OSCPM_OSCPM_HPP_INCLUDED
