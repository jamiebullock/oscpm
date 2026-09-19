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
#include <cstdint>
#include <optional>
#include <string_view>

namespace oscpm {

// Why an operation failed: either why an input is Malformed, or, for
// Duplicate and NotFound, why AddressSpace refused a registration or a
// removal. Vocabulary follows CONTEXT.md.
enum class ErrorKind : unsigned char {
    MissingLeadingSlash, // The first byte is not '/'.
    BareRoot,            // The input is exactly "/".
    TrailingSlash,       // The input ends in '/'.
    EmptyPart,           // Two adjacent '/' in an Address, or three in a Pattern,
                         // where "//" is the Descendant Operator.
    IllegalCharacter,    // A pattern character in an Address, '#' or space anywhere,
                         // or a ']', '}' or ',' outside its construct in a Pattern.
    EmptyCharacterClass, // "[]" or "[!]".
    UnterminatedCharacterClass, // A '[' with no ']' in the same Part.
    UnterminatedAlternative,    // A '{' with no '}' in the same Part.
    NestedAlternative,          // A '{' inside an Alternative.
    BracketInAlternative,       // A '[' or ']' inside an Alternative.
    PartTooLong,                // A Pattern Part longer than maxPatternPartLength.
    Duplicate,                  // A Method is already registered at the Address.
    NotFound,                   // No Method is registered at the Address.
};

// The longest Pattern Part that parses, in bytes. Matching tracks a set of
// byte positions within one Part on the stack (ADR 0004); this bounds that
// set. Addresses have no such limit.
constexpr std::size_t maxPatternPartLength = 8191;

// What went wrong and the zero-based byte offset of the byte that made the
// input Malformed. The offset is 0 for Duplicate and NotFound, where no byte
// is at fault.
struct Error {
    ErrorKind kind;
    std::size_t offset;
};

// The enumerator's name, for diagnostics.
constexpr const char* toString(ErrorKind kind) noexcept
{
    switch (kind) {
    case ErrorKind::MissingLeadingSlash: return "MissingLeadingSlash";
    case ErrorKind::BareRoot: return "BareRoot";
    case ErrorKind::TrailingSlash: return "TrailingSlash";
    case ErrorKind::EmptyPart: return "EmptyPart";
    case ErrorKind::IllegalCharacter: return "IllegalCharacter";
    case ErrorKind::EmptyCharacterClass: return "EmptyCharacterClass";
    case ErrorKind::UnterminatedCharacterClass: return "UnterminatedCharacterClass";
    case ErrorKind::UnterminatedAlternative: return "UnterminatedAlternative";
    case ErrorKind::NestedAlternative: return "NestedAlternative";
    case ErrorKind::BracketInAlternative: return "BracketInAlternative";
    case ErrorKind::PartTooLong: return "PartTooLong";
    case ErrorKind::Duplicate: return "Duplicate";
    case ErrorKind::NotFound: return "NotFound";
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

// The bytes OSC 1.0 reserves outside of pattern syntax. A Pattern is an
// Address whose Parts may contain Wildcards, so neither side may carry them.
constexpr bool isReserved(char c) noexcept
{
    return c == '#' || c == ' ';
}

// One left-to-right scan of a literal Address: a leading '/', at least one
// Part, no empty Part, no trailing '/', no reserved or pattern bytes. The
// first fault by byte offset wins.
constexpr std::optional<Error> validateAddress(std::string_view text) noexcept
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
            return Error{ErrorKind::EmptyPart, i};
        }
        if (isReserved(c) || isPatternCharacter(c)) {
            return Error{ErrorKind::IllegalCharacter, i};
        }
    }
    if (text.back() == '/') {
        return Error{ErrorKind::TrailingSlash, text.size() - 1};
    }
    return std::nullopt;
}

// One left-to-right scan of a Pattern: the structural rules shared with
// Addresses, the Descendant Operator, and the within-Part Wildcard syntax.
// The first fault by byte offset wins; an unterminated '[' or '{' faults at
// its opening byte and so is reported before anything inside it.
constexpr std::optional<Error> scanPatternSyntax(std::string_view text) noexcept
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
            // "//" is the Descendant Operator (ADR 0002). A third '/' closes
            // an empty Part; a trailing "//" is caught by the final check.
            if (i >= 2 && text[i - 2] == '/') {
                return Error{ErrorKind::EmptyPart, i};
            }
            continue;
        }
        if (isReserved(c)) {
            return Error{ErrorKind::IllegalCharacter, i};
        }
        if (c == '?' || c == '*') {
            continue;
        }
        if (c == '[') {
            // The class body runs to the first ']' after an optional '!'. It
            // cannot cross '/', since a Wildcard never matches one, so a '/'
            // or the end of the input first means the '[' is unterminated.
            std::size_t j = i + 1;
            if (j < text.size() && text[j] == '!') {
                ++j;
            }
            if (j < text.size() && text[j] == ']') {
                return Error{ErrorKind::EmptyCharacterClass, j};
            }
            while (j < text.size() && text[j] != ']' && text[j] != '/') {
                ++j;
            }
            if (j == text.size() || text[j] != ']') {
                return Error{ErrorKind::UnterminatedCharacterClass, i};
            }
            for (std::size_t k = i + 1; k < j; ++k) {
                if (isReserved(text[k])) {
                    return Error{ErrorKind::IllegalCharacter, k};
                }
            }
            i = j;
            continue;
        }
        if (c == '{') {
            // As with '[', the body runs to the first '}' in the same Part and
            // an unterminated '{' is reported before anything inside it.
            std::size_t j = i + 1;
            while (j < text.size() && text[j] != '}' && text[j] != '/') {
                ++j;
            }
            if (j == text.size() || text[j] != '}') {
                return Error{ErrorKind::UnterminatedAlternative, i};
            }
            for (std::size_t k = i + 1; k < j; ++k) {
                const char inner = text[k];
                if (inner == '{') {
                    return Error{ErrorKind::NestedAlternative, k};
                }
                if (inner == '[' || inner == ']') {
                    return Error{ErrorKind::BracketInAlternative, k};
                }
                if (isReserved(inner)) {
                    return Error{ErrorKind::IllegalCharacter, k};
                }
            }
            i = j;
            continue;
        }
        if (c == ']' || c == '}' || c == ',') {
            return Error{ErrorKind::IllegalCharacter, i};
        }
    }
    if (text.back() == '/') {
        return Error{ErrorKind::TrailingSlash, text.size() - 1};
    }
    return std::nullopt;
}

// Finds the first Part longer than maxPatternPartLength. The fault is the
// first byte beyond the limit.
constexpr std::optional<Error> findOverlongPart(std::string_view text) noexcept
{
    std::size_t partStart = 1;
    for (std::size_t i = 1; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '/') {
            if (i - partStart > maxPatternPartLength) {
                return Error{ErrorKind::PartTooLong, partStart + maxPatternPartLength};
            }
            partStart = i + 1;
        }
    }
    return std::nullopt;
}

// Validates a Pattern. Of the syntax fault and the length fault, the one at
// the lower byte offset is reported; at the same byte the syntax fault is,
// being the more specific.
constexpr std::optional<Error> validatePattern(std::string_view text) noexcept
{
    const std::optional<Error> syntax = scanPatternSyntax(text);
    if (text.size() <= maxPatternPartLength + 1) {
        return syntax; // No Part can be too long.
    }
    const std::optional<Error> overlong = findOverlongPart(text);
    if (syntax && overlong) {
        return syntax->offset <= overlong->offset ? syntax : overlong;
    }
    return syntax ? syntax : overlong;
}

// Tests `byte` against the body of a Character Class: the bytes between
// '[' and ']', which validatePattern has checked is non-empty after any
// leading '!'. An element "x-y" is a range in byte order. A '-' is literal
// when it is the first element or the last byte, so it neither opens nor
// closes a range there; a reversed range matches nothing.
constexpr bool classMatches(std::string_view body, unsigned char byte) noexcept
{
    bool negate = false;
    std::size_t i = 0;
    if (body.front() == '!') {
        negate = true;
        i = 1;
    }
    const std::size_t first = i;
    bool found = false;
    while (i < body.size()) {
        const unsigned char low = static_cast<unsigned char>(body[i]);
        unsigned char high = low;
        const bool opensRange = !(i == first && body[i] == '-');
        const bool closesRange = i + 2 < body.size() && body[i + 1] == '-'
                                 && !(i + 2 == body.size() - 1 && body[i + 2] == '-');
        if (opensRange && closesRange) {
            high = static_cast<unsigned char>(body[i + 2]);
            i += 3;
        } else {
            ++i;
        }
        if (low <= byte && byte <= high) {
            found = true;
        }
    }
    return found != negate;
}

// A set of byte positions within one Pattern Part, 0 to maxPatternPartLength
// inclusive so that the position just past a maximal Part fits.
class PositionSet {
public:
    constexpr bool test(std::size_t i) const noexcept
    {
        return ((m_words[i / 64] >> (i % 64)) & 1u) != 0;
    }
    constexpr void add(std::size_t i) noexcept
    {
        m_words[i / 64] |= std::uint64_t{1} << (i % 64);
    }
    constexpr void clear() noexcept
    {
        for (std::uint64_t& word : m_words) {
            word = 0;
        }
    }
    constexpr bool empty() const noexcept
    {
        for (const std::uint64_t word : m_words) {
            if (word != 0) {
                return false;
            }
        }
        return true;
    }

private:
    static constexpr std::size_t words = (maxPatternPartLength + 1 + 63) / 64;
    std::uint64_t m_words[words] = {};
};

// Matching one Part is a set simulation rather than a backtracking search
// (ADR 0004). A state is a byte position in the Pattern Part; the set holds
// every position that some reading of the Address bytes consumed so far
// could have reached. Each Address byte advances the whole set at once, so
// the cost is bounded by the product of the two Part lengths whatever the
// Pattern contains, with no recursion and no heap.
//
// Reading Address byte `b` at position p moves a state to p + 1 when
// pattern[p] is '?' or equals b, to just past the ']' when p opens a
// Character Class containing b, and keeps it at p when pattern[p] is '*'.
// Empty moves, applied by `followEmptyMoves`, jump from '{' to every
// member's first byte, from a member's end at ',' or '}' to just past the
// '}', and from '*' to p + 1. Inside braces every byte is literal.

// Adds every position reachable from `set` by empty moves. Empty moves only
// go forward, so one increasing pass over the Part reaches all of them.
constexpr void followEmptyMoves(PositionSet& set, std::string_view pattern) noexcept
{
    bool inBraces = false;
    std::size_t braceClose = 0; // The '}' of the braces being walked.
    for (std::size_t p = 0; p < pattern.size(); ++p) {
        const char c = pattern[p];
        if (inBraces) {
            if (c == '}') {
                inBraces = false;
                if (set.test(p)) {
                    set.add(p + 1);
                }
            } else if (c == ',' && set.test(p)) {
                set.add(braceClose + 1);
            }
            continue;
        }
        if (c == '[') {
            p = pattern.find(']', p + 1);
            continue;
        }
        if (c == '{') {
            inBraces = true;
            braceClose = pattern.find('}', p + 1);
            if (set.test(p)) {
                set.add(p + 1);
                for (std::size_t q = p + 1; q < braceClose; ++q) {
                    if (pattern[q] == ',') {
                        set.add(q + 1);
                    }
                }
            }
            continue;
        }
        if (c == '*' && set.test(p)) {
            set.add(p + 1);
        }
    }
}

// Fills `to` with every position reachable from `from` by reading `byte`.
constexpr void readByte(const PositionSet& from, PositionSet& to, std::string_view pattern,
                        unsigned char byte) noexcept
{
    to.clear();
    bool inBraces = false;
    for (std::size_t p = 0; p < pattern.size(); ++p) {
        const char c = pattern[p];
        if (inBraces) {
            if (c == '}') {
                inBraces = false;
            } else if (c != ',' && from.test(p) && static_cast<unsigned char>(c) == byte) {
                to.add(p + 1);
            }
            continue;
        }
        if (c == '[') {
            const std::size_t close = pattern.find(']', p + 1);
            if (from.test(p) && classMatches(pattern.substr(p + 1, close - p - 1), byte)) {
                to.add(close + 1);
            }
            p = close;
            continue;
        }
        if (c == '{') {
            inBraces = true;
            continue;
        }
        if (from.test(p)) {
            if (c == '*') {
                to.add(p);
            } else if (c == '?' || static_cast<unsigned char>(c) == byte) {
                to.add(p + 1);
            }
        }
    }
}

// Tests one Part of a well-formed Pattern, no longer than
// maxPatternPartLength, against one Part of a well-formed Address. Neither
// view contains '/'.
constexpr bool matchPart(std::string_view pattern, std::string_view address) noexcept
{
    PositionSet sets[2];
    std::size_t current = 0;
    sets[current].add(0);
    followEmptyMoves(sets[current], pattern);
    for (const char c : address) {
        readByte(sets[current], sets[1 - current], pattern, static_cast<unsigned char>(c));
        current = 1 - current;
        if (sets[current].empty()) {
            return false;
        }
        followEmptyMoves(sets[current], pattern);
    }
    return sets[current].test(pattern.size());
}

// The byte just past the Part beginning at `start`: the next '/' or the
// end of the text.
constexpr std::size_t partEnd(std::string_view text, std::size_t start) noexcept
{
    const std::size_t slash = text.find('/', start);
    return slash == std::string_view::npos ? text.size() : slash;
}

// Where a Part-by-Part walk of a Pattern over an Address stands between two
// Address Parts. `matchParts` advances it over the Parts of one Address;
// AddressSpace carries one per Container so that a child continues from
// where its parent stopped rather than rematching the whole Address.
struct MatchState {
    static constexpr std::size_t none = std::string_view::npos;

    std::size_t p = 1;          // The first byte of the current Pattern Part.
    std::size_t resumeP = none; // The Part after the most recent "//".
    std::size_t resumeA = none; // The Address Part that "//" currently ends before.

    // Whether the Address consumed so far is one the Pattern Matches.
    constexpr bool complete(std::string_view pattern) const noexcept
    {
        return p >= pattern.size();
    }

    // Whether a longer Address could still Match once the Pattern is used
    // up: only if a "//" has been passed, since it can absorb more Parts.
    constexpr bool canAbsorbMore() const noexcept { return resumeP != none; }
};

// Advances `state` over the Parts of a well-formed Address from byte `a` to
// the end of `address`, against a well-formed Pattern. Returns false once
// no Address beginning with `address` can Match: a Part failed and no "//"
// before it can absorb more. Returns true otherwise, with `state` ready for
// the Parts that follow, and `state.complete(pattern)` saying whether
// `address` itself Matches.
//
// The Descendant Operator matches zero or more whole Parts. Each "//" is
// first taken to match zero Parts and the Parts after it are matched
// greedily; when one of them fails, or the Pattern runs out before the
// Address does, the walk resumes just after the most recent "//" with that
// operator absorbing one more Address Part. Only the most recent "//" need
// be revisited: an earlier one could only absorb Parts that the later one
// can absorb instead. This is the single-restart-point walk that filename
// matchers use for '*', and it bounds the work by the product of the two
// Part counts with no recursion, so no Pattern can send it exponential.
//
// Because the walk reads Address bytes left to right and only ever resumes
// from a Part it has already passed, its state on reaching the end of an
// Address is the state it would be in at that same byte of any longer
// Address with the same prefix; that is what lets a tree walk carry it.
constexpr bool matchParts(MatchState& state, std::string_view pattern, std::string_view address,
                          std::size_t a) noexcept
{
    for (;;) {
        if (state.p < pattern.size() && pattern[state.p] == '/') {
            state.resumeP = ++state.p;
            state.resumeA = a;
            continue;
        }
        if (a >= address.size()) {
            return true;
        }
        if (state.p < pattern.size()) {
            const std::size_t patternEnd = partEnd(pattern, state.p);
            const std::size_t addressEnd = partEnd(address, a);
            if (matchPart(pattern.substr(state.p, patternEnd - state.p),
                          address.substr(a, addressEnd - a))) {
                state.p = patternEnd + 1;
                a = addressEnd + 1;
                continue;
            }
        }
        if (state.resumeP == MatchState::none) {
            return false;
        }
        state.resumeA = partEnd(address, state.resumeA) + 1;
        state.p = state.resumeP;
        a = state.resumeA;
    }
}

// Tests a well-formed Pattern against a well-formed Address.
constexpr bool matchAddress(std::string_view pattern, std::string_view address) noexcept
{
    MatchState state;
    return matchParts(state, pattern, address, 1) && state.complete(pattern);
}

} // namespace detail

// Checks that `address` is a well-formed literal Address.
constexpr std::optional<Error> validateAddress(std::string_view address) noexcept
{
    return detail::validateAddress(address);
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
        return detail::matchAddress(m_text, address) ? MatchResult::Match : MatchResult::NoMatch;
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
    if (const std::optional<Error> error = detail::validatePattern(text)) {
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
