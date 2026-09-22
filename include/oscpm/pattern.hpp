// oscpm - OSC address pattern matching for C++
//
// Copyright (c) 2026 Jamie Bullock
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace oscpm {

/// Reasons a pattern or address can fail validation.
enum class ErrorCode
{
    none = 0,
    empty,                    ///< The string is empty.
    missing_leading_slash,    ///< The string does not start with '/'.
    empty_part,               ///< An empty path part: a trailing '/', "//" in an address, or "///" in a pattern.
    unterminated_bracket,     ///< '[' without a closing ']'.
    unterminated_brace,       ///< '{' without a closing '}'.
    nested_brace,             ///< '{' inside a '{...}' list.
    unexpected_close_bracket, ///< ']' outside a '[...]' class.
    unexpected_close_brace,   ///< '}' outside a '{...}' list.
    slash_in_bracket,         ///< '/' inside a '[...]' class.
    slash_in_brace,           ///< '/' inside a '{...}' list.
    reversed_range,           ///< A range such as "[z-a]" whose start is greater than its end.
    reserved_character,       ///< An address contains one of the reserved characters " #*,?[]{}".
};

/// A human readable description of an error code.
constexpr const char* message(ErrorCode code) noexcept
{
    switch (code) {
        case ErrorCode::none: return "no error";
        case ErrorCode::empty: return "string is empty";
        case ErrorCode::missing_leading_slash: return "must start with '/'";
        case ErrorCode::empty_part: return "empty path part";
        case ErrorCode::unterminated_bracket: return "'[' without matching ']'";
        case ErrorCode::unterminated_brace: return "'{' without matching '}'";
        case ErrorCode::nested_brace: return "'{' nested inside '{...}'";
        case ErrorCode::unexpected_close_bracket: return "']' without matching '['";
        case ErrorCode::unexpected_close_brace: return "'}' without matching '{'";
        case ErrorCode::slash_in_bracket: return "'/' inside '[...]'";
        case ErrorCode::slash_in_brace: return "'/' inside '{...}'";
        case ErrorCode::reversed_range: return "reversed character range in '[...]'";
        case ErrorCode::reserved_character: return "reserved character in address";
    }
    return "unknown error";
}

/// The result of validating a pattern or address. Converts to `true` when it holds an error.
struct Error
{
    ErrorCode code = ErrorCode::none;
    std::size_t position = 0; ///< Byte offset into the validated string.

    constexpr explicit operator bool() const noexcept { return code != ErrorCode::none; }
    constexpr const char* message() const noexcept { return oscpm::message(code); }
};

constexpr bool operator==(Error a, Error b) noexcept { return a.code == b.code && a.position == b.position; }
constexpr bool operator!=(Error a, Error b) noexcept { return !(a == b); }

/// True for the characters that OSC 1.0 forbids in address parts: " #*,?[]{}".
constexpr bool is_reserved(char c) noexcept
{
    return c == ' ' || c == '#' || c == '*' || c == ',' || c == '?' || c == '[' || c == ']' || c == '{' || c == '}';
}

namespace detail {

    constexpr std::size_t npos = std::string_view::npos;

    constexpr unsigned char uc(char c) noexcept { return static_cast<unsigned char>(c); }

    // Matches `ch` against the character class starting at p[pos] == '['.
    // On return `pos` is the index just past the closing ']' (or p.size() if unterminated).
    constexpr bool match_class(std::string_view p, std::size_t& pos, char ch) noexcept
    {
        std::size_t i = pos + 1;
        bool negate = false;
        if (i < p.size() && p[i] == '!') {
            negate = true;
            ++i;
        }
        bool matched = false;
        while (i < p.size() && p[i] != ']') {
            if (i + 2 < p.size() && p[i + 1] == '-' && p[i + 2] != ']') {
                if (uc(p[i]) <= uc(ch) && uc(ch) <= uc(p[i + 2]))
                    matched = true;
                i += 3;
            } else {
                if (p[i] == ch)
                    matched = true;
                ++i;
            }
        }
        pos = i < p.size() ? i + 1 : i;
        return matched != negate;
    }

    // Matches one path part against one pattern part. Neither contains '/'.
    //
    // '*' is handled iteratively with a single backtrack point (the most recent star), which is
    // sufficient because a later '*' can always absorb whatever an earlier one could. '{...}' lists
    // are the only construct with more than one way to proceed, so they recurse on the remainder.
    constexpr bool match_part(std::string_view p, std::string_view a) noexcept
    {
        std::size_t pi = 0;
        std::size_t ai = 0;
        std::size_t star_pi = npos;
        std::size_t star_ai = 0;

        for (;;) {
            if (pi == p.size()) {
                if (ai == a.size())
                    return true;
            } else {
                switch (p[pi]) {
                    case '*':
                        while (pi < p.size() && p[pi] == '*')
                            ++pi;
                        star_pi = pi;
                        star_ai = ai;
                        continue;

                    case '?':
                        if (ai < a.size()) {
                            ++pi;
                            ++ai;
                            continue;
                        }
                        break;

                    case '[': {
                        if (ai < a.size()) {
                            std::size_t next = pi;
                            if (match_class(p, next, a[ai])) {
                                pi = next;
                                ++ai;
                                continue;
                            }
                        }
                        break;
                    }

                    case '{': {
                        std::size_t close = p.find('}', pi + 1);
                        if (close == npos)
                            close = p.size();
                        const std::string_view rest = close < p.size() ? p.substr(close + 1) : std::string_view{};
                        const std::string_view tail = a.substr(ai);
                        std::size_t alt_begin = pi + 1;
                        for (;;) {
                            std::size_t alt_end = p.find(',', alt_begin);
                            if (alt_end == npos || alt_end > close)
                                alt_end = close;
                            const std::string_view alt = p.substr(alt_begin, alt_end - alt_begin);
                            if (tail.substr(0, alt.size()) == alt && match_part(rest, tail.substr(alt.size())))
                                return true;
                            if (alt_end == close)
                                break;
                            alt_begin = alt_end + 1;
                        }
                        break;
                    }

                    default:
                        if (ai < a.size() && p[pi] == a[ai]) {
                            ++pi;
                            ++ai;
                            continue;
                        }
                        break;
                }
            }

            // Mismatch: retry from the most recent '*' with one more character consumed.
            if (star_pi == npos || star_ai == a.size())
                return false;
            pi = star_pi;
            ai = ++star_ai;
        }
    }

    // Matches a whole path. `p` and `a` are each either empty or begin with '/'.
    constexpr bool match_path(std::string_view p, std::string_view a) noexcept
    {
        for (;;) {
            if (p.empty())
                return a.empty();
            if (a.empty())
                return false;

            if (p.size() > 1 && p[1] == '/') {
                // OSC 1.1 "//": the remainder may match at any part boundary, including this one.
                const std::string_view rest = p.substr(1);
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (a[i] == '/' && match_path(rest, a.substr(i)))
                        return true;
                }
                return false;
            }

            std::size_t pe = p.find('/', 1);
            if (pe == npos)
                pe = p.size();
            std::size_t ae = a.find('/', 1);
            if (ae == npos)
                ae = a.size();

            if (!match_part(p.substr(1, pe - 1), a.substr(1, ae - 1)))
                return false;

            p.remove_prefix(pe);
            a.remove_prefix(ae);
        }
    }

} // namespace detail

/// Matches an OSC address pattern against a literal OSC address.
///
/// Supports the OSC 1.0 pattern syntax ('?', '*', '[...]', '{...}') and the OSC 1.1 "//" operator.
/// Wildcards never match '/'; each pattern part matches exactly one address part, except "//" which
/// matches zero or more parts. No validation is performed and nothing is allocated. `address` is
/// assumed to be a well formed literal address (see `validate_address`). A malformed pattern or
/// address never causes undefined behaviour, but what matches is unspecified; use
/// `validate_pattern` or `Pattern` for patterns that need checking.
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    if (pattern.empty() || address.empty() || pattern[0] != '/' || address[0] != '/')
        return false;
    return detail::match_path(pattern, address);
}

/// Checks that `pattern` is a well formed OSC address pattern.
constexpr Error validate_pattern(std::string_view p) noexcept
{
    if (p.empty())
        return {ErrorCode::empty, 0};
    if (p[0] != '/')
        return {ErrorCode::missing_leading_slash, 0};

    const std::size_t n = p.size();
    std::size_t i = 0;
    while (i < n) {
        switch (p[i]) {
            case '/': {
                std::size_t run = 1;
                while (i + run < n && p[i + run] == '/')
                    ++run;
                if (run > 2)
                    return {ErrorCode::empty_part, i + 2};
                if (i + run == n)
                    return {ErrorCode::empty_part, i + run};
                i += run;
                break;
            }
            case '[': {
                const std::size_t start = i++;
                if (i < n && p[i] == '!')
                    ++i;
                while (i < n && p[i] != ']') {
                    if (p[i] == '/')
                        return {ErrorCode::slash_in_bracket, i};
                    if (i + 2 < n && p[i + 1] == '-' && p[i + 2] != ']') {
                        if (p[i + 2] == '/')
                            return {ErrorCode::slash_in_bracket, i + 2};
                        if (detail::uc(p[i]) > detail::uc(p[i + 2]))
                            return {ErrorCode::reversed_range, i};
                        i += 3;
                    } else {
                        ++i;
                    }
                }
                if (i == n)
                    return {ErrorCode::unterminated_bracket, start};
                ++i;
                break;
            }
            case ']':
                return {ErrorCode::unexpected_close_bracket, i};
            case '{': {
                const std::size_t start = i++;
                while (i < n && p[i] != '}') {
                    if (p[i] == '/')
                        return {ErrorCode::slash_in_brace, i};
                    if (p[i] == '{')
                        return {ErrorCode::nested_brace, i};
                    ++i;
                }
                if (i == n)
                    return {ErrorCode::unterminated_brace, start};
                ++i;
                break;
            }
            case '}':
                return {ErrorCode::unexpected_close_brace, i};
            default:
                ++i;
                break;
        }
    }
    return {};
}

/// Checks that `address` is a well formed literal OSC address: it starts with '/', has no empty
/// parts and contains none of the reserved characters " #*,?[]{}".
constexpr Error validate_address(std::string_view a) noexcept
{
    if (a.empty())
        return {ErrorCode::empty, 0};
    if (a[0] != '/')
        return {ErrorCode::missing_leading_slash, 0};
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char c = a[i];
        if (c == '/') {
            if (i + 1 == a.size() || a[i + 1] == '/')
                return {ErrorCode::empty_part, i + 1};
        } else if (is_reserved(c)) {
            return {ErrorCode::reserved_character, i};
        }
    }
    return {};
}

/// The longest prefix of `pattern` that every matching address must start with, i.e. everything up
/// to the first wildcard or "//". For a literal pattern this is the whole pattern.
constexpr std::string_view literal_prefix(std::string_view pattern) noexcept
{
    std::size_t end = pattern.find_first_of("*?[{");
    if (end == detail::npos)
        end = pattern.size();
    const std::size_t dbl = pattern.find("//");
    if (dbl != detail::npos && dbl + 1 < end)
        end = dbl + 1;
    return pattern.substr(0, end);
}

/// True if `pattern` contains no wildcards and no "//", so it can only match itself.
constexpr bool is_literal(std::string_view pattern) noexcept
{
    return literal_prefix(pattern).size() == pattern.size();
}

/// Thrown by `Pattern`'s constructor for a malformed pattern.
class PatternError : public std::invalid_argument
{
public:
    PatternError(std::string_view pattern, Error error)
    : std::invalid_argument(build_message(pattern, error))
    , m_error(error)
    {
    }

    Error error() const noexcept { return m_error; }

private:
    static std::string build_message(std::string_view pattern, Error error)
    {
        std::string s = "invalid OSC address pattern \"";
        s.append(pattern);
        s += "\": ";
        s += error.message();
        s += " at offset ";
        s += std::to_string(error.position);
        return s;
    }

    Error m_error;
};

/// A validated OSC address pattern.
///
/// Compiling a pattern checks it once and records whether it is literal, so `matches` can take the
/// fastest route. `Pattern` owns its string; matching itself never allocates.
class Pattern
{
public:
    /// Compiles `pattern`, or returns an empty optional if it is malformed. Use `validate_pattern`
    /// to find out why.
    static std::optional<Pattern> compile(std::string_view pattern)
    {
        if (validate_pattern(pattern))
            return std::nullopt;
        return Pattern(checked{}, pattern);
    }

    /// Compiles `pattern`, throwing `PatternError` if it is malformed.
    explicit Pattern(std::string_view pattern)
    : Pattern(checked{}, throw_if_invalid(pattern))
    {
    }

    /// True if `address` matches this pattern.
    bool matches(std::string_view address) const noexcept
    {
        return m_literal ? std::string_view(m_str) == address : match(m_str, address);
    }

    /// The pattern text.
    const std::string& str() const noexcept { return m_str; }

    /// True if the pattern contains no wildcards, so it can only match itself.
    bool is_literal() const noexcept { return m_literal; }

    /// See `oscpm::literal_prefix`.
    std::string_view literal_prefix() const noexcept { return std::string_view(m_str).substr(0, m_prefix); }

    friend bool operator==(const Pattern& a, const Pattern& b) noexcept { return a.m_str == b.m_str; }
    friend bool operator!=(const Pattern& a, const Pattern& b) noexcept { return !(a == b); }

private:
    struct checked
    {
    };

    Pattern(checked, std::string_view pattern)
    : m_str(pattern)
    , m_prefix(oscpm::literal_prefix(pattern).size())
    , m_literal(m_prefix == pattern.size())
    {
    }

    static std::string_view throw_if_invalid(std::string_view pattern)
    {
        if (const Error error = validate_pattern(pattern))
            throw PatternError(pattern, error);
        return pattern;
    }

    std::string m_str;
    std::size_t m_prefix;
    bool m_literal;
};

/// Matches a compiled pattern against an address.
inline bool match(const Pattern& pattern, std::string_view address) noexcept
{
    return pattern.matches(address);
}

} // namespace oscpm
