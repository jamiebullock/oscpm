// oscpm: OSC address pattern matching
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

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace oscpm {

// Why an input is Malformed. Vocabulary follows CONTEXT.md.
enum class ErrorKind : unsigned char {
    MissingLeadingSlash, // The first byte is not '/'.
    BareRoot,            // The input is exactly "/".
    TrailingSlash,       // The input ends in '/'.
    EmptyPart,           // Two adjacent '/' in an Address.
    IllegalCharacter,    // An Address contains a pattern character, '#' or space.
    NotYetSupported,     // A Pattern contains a Wildcard or "//". Temporary.
};

// A Malformed result: what went wrong and the zero-based byte offset of the
// byte that made the input Malformed.
struct Error {
    ErrorKind kind;
    std::size_t offset;
};

// The enumerator's name, for diagnostics.
constexpr const char* name(ErrorKind kind) noexcept
{
    switch (kind) {
    case ErrorKind::MissingLeadingSlash: return "MissingLeadingSlash";
    case ErrorKind::BareRoot: return "BareRoot";
    case ErrorKind::TrailingSlash: return "TrailingSlash";
    case ErrorKind::EmptyPart: return "EmptyPart";
    case ErrorKind::IllegalCharacter: return "IllegalCharacter";
    case ErrorKind::NotYetSupported: return "NotYetSupported";
    }
    return "?";
}

// The outcome of testing one Pattern against one Address.
enum class MatchResult : unsigned char {
    Match,
    NoMatch,
    Malformed, // The Address was Malformed; see validateAddress for details.
};

namespace detail {

constexpr bool isPatternCharacter(char c) noexcept
{
    return c == '?' || c == '*' || c == '[' || c == ']' || c == '{' || c == '}' || c == ',';
}

// The bytes OSC 1.0 forbids in the name of a Container or Method.
constexpr bool isIllegalInAddress(char c) noexcept
{
    return isPatternCharacter(c) || c == '#' || c == ' ';
}

// One left-to-right scan of the rules shared by Addresses and Patterns: a
// leading '/', at least one Part, no trailing '/'. The first fault by byte
// offset wins. In an Address, two adjacent slashes are an empty Part and
// the OSC 1.0 reserved bytes are illegal; in a Pattern, until later tickets
// implement them, "//" and the pattern characters are unsupported.
constexpr std::optional<Error> validate(std::string_view text, bool isAddress) noexcept
{
    if (text.empty() || text.front() != '/') {
        return Error{ErrorKind::MissingLeadingSlash, 0};
    }
    if (text.size() == 1) {
        return Error{ErrorKind::BareRoot, 0};
    }
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '/' && text[i - 1] == '/') {
            return Error{isAddress ? ErrorKind::EmptyPart : ErrorKind::NotYetSupported, i};
        }
        if (isAddress ? isIllegalInAddress(c) : isPatternCharacter(c)) {
            return Error{isAddress ? ErrorKind::IllegalCharacter : ErrorKind::NotYetSupported, i};
        }
    }
    if (text.back() == '/') {
        return Error{ErrorKind::TrailingSlash, text.size() - 1};
    }
    return std::nullopt;
}

} // namespace detail

// Checks that `address` is a well-formed literal Address.
constexpr std::optional<Error> validateAddress(std::string_view address) noexcept
{
    return detail::validate(address, true);
}

class ParseResult;

// A validated OSC address pattern. Holds only a view of the caller's bytes,
// which must outlive every use of the Pattern (ADR 0003).
class Pattern {
public:
    // Validates `text` once. The returned Pattern's `matches` never reports
    // the Pattern itself as Malformed.
    static constexpr ParseResult parse(std::string_view text) noexcept;

    // Tests this Pattern against a literal Address. Malformed arises only
    // from the Address side.
    constexpr MatchResult matches(std::string_view address) const noexcept
    {
        if (validateAddress(address)) {
            return MatchResult::Malformed;
        }
        return address == m_text ? MatchResult::Match : MatchResult::NoMatch;
    }

    // The bytes this Pattern was parsed from.
    constexpr std::string_view text() const noexcept { return m_text; }

private:
    friend class ParseResult;

    constexpr Pattern() noexcept = default;
    constexpr explicit Pattern(std::string_view text) noexcept : m_text(text) {}

    std::string_view m_text;
};

// Either a Pattern or the Error that stopped it parsing.
class ParseResult {
public:
    constexpr bool ok() const noexcept { return m_ok; }
    constexpr explicit operator bool() const noexcept { return m_ok; }

    // Precondition: ok().
    constexpr const Pattern& pattern() const noexcept { return m_pattern; }

    // Precondition: !ok().
    constexpr const Error& error() const noexcept { return m_error; }

private:
    friend class Pattern;

    constexpr explicit ParseResult(Pattern pattern) noexcept : m_ok(true), m_pattern(pattern) {}
    constexpr explicit ParseResult(Error error) noexcept : m_ok(false), m_error(error) {}

    bool m_ok;
    Pattern m_pattern;
    Error m_error{ErrorKind::MissingLeadingSlash, 0};
};

constexpr ParseResult Pattern::parse(std::string_view text) noexcept
{
    if (const std::optional<Error> error = detail::validate(text, false)) {
        return ParseResult(*error);
    }
    return ParseResult(Pattern(text));
}

// Parses `pattern` and tests it against `address` in one call. Malformed
// may arise from either side; use Pattern::parse and validateAddress to
// learn which.
constexpr MatchResult match(std::string_view pattern, std::string_view address) noexcept
{
    const ParseResult parsed = Pattern::parse(pattern);
    if (!parsed.ok()) {
        return MatchResult::Malformed;
    }
    return parsed.pattern().matches(address);
}

} // namespace oscpm
