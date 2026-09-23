/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace oscpm
{

/// The longest address part `match` compares. A longer address part never
/// matches, and `validateAddress` reports it as `PartTooLong`.
constexpr std::size_t kMaxAddressPartLength = 4095;

/// The longest pattern containing '*', '?', '[', '{' or "//" that
/// `validatePattern` accepts. A longer such pattern is reported as
/// `PatternTooLong` and matches nothing; a literal pattern has no limit.
constexpr std::size_t kMaxPatternLength = 1024;

/// A fault in a pattern, in an address or in an `AddressSpace` operation
/// (the last two). `MissingLeadingSlash` is reported for both a pattern
/// and an address; `UnterminatedClass`, `UnterminatedBraces` and
/// `PatternTooLong` only for a pattern; the four that follow them only for an
/// address.
enum class Error
{
    MissingLeadingSlash,
    UnterminatedClass,
    UnterminatedBraces,
    PatternTooLong,
    TrailingSlash,
    EmptyPart,
    IllegalByte,
    PartTooLong,
    Duplicate,
    NotFound
};

/// A fault and the zero-based byte offset at which it was found.
struct ParseError
{
    Error kind;
    std::size_t offset;
};

/// The enumerator's name, for diagnostics.
constexpr const char* toString(Error error) noexcept
{
    switch (error)
    {
    case Error::MissingLeadingSlash:
        return "MissingLeadingSlash";
    case Error::UnterminatedClass:
        return "UnterminatedClass";
    case Error::UnterminatedBraces:
        return "UnterminatedBraces";
    case Error::PatternTooLong:
        return "PatternTooLong";
    case Error::TrailingSlash:
        return "TrailingSlash";
    case Error::EmptyPart:
        return "EmptyPart";
    case Error::IllegalByte:
        return "IllegalByte";
    case Error::PartTooLong:
        return "PartTooLong";
    case Error::Duplicate:
        return "Duplicate";
    case Error::NotFound:
        return "NotFound";
    }
    return "";
}

}

namespace oscpm::detail
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
    constexpr std::string_view reservedInAddress = "#*,?[]{}";
    constexpr unsigned char firstPrintableAscii = 0x21;
    constexpr unsigned char lastPrintableAscii = 0x7E;
    constexpr std::size_t operatorRunLength = 2;
    constexpr std::string_view descendantOperator = "//";
    constexpr std::size_t bitsPerWord = 64;
    constexpr std::size_t reachWords = kMaxAddressPartLength / bitsPerWord + 1;
    constexpr std::size_t smallReachWords = 1;
}

constexpr std::size_t npos = std::string_view::npos;

template <std::size_t NumWords>
class Reach
{
public:
    constexpr explicit Reach(std::size_t partLength) noexcept
        : m_numWords(partLength / k::bitsPerWord + 1)
    {
    }

    constexpr bool test(std::size_t position) const noexcept
    {
        return ((m_words[position / k::bitsPerWord] >> (position % k::bitsPerWord)) & 1U) != 0U;
    }

    constexpr void set(std::size_t position) noexcept
    {
        m_words[position / k::bitsPerWord] |= std::uint64_t { 1 } << (position % k::bitsPerWord);
    }

    constexpr void setFrom(std::size_t first, std::size_t last) noexcept
    {
        for (std::size_t position = first; position <= last; ++position)
        {
            set(position);
        }
    }

    constexpr bool empty() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                return false;
            }
        }
        return true;
    }

    constexpr std::size_t lowest() const noexcept
    {
        for (std::size_t word = 0; word < m_numWords; ++word)
        {
            if (m_words[word] != 0U)
            {
                std::size_t position = word * k::bitsPerWord;
                while (!test(position))
                {
                    ++position;
                }
                return position;
            }
        }
        return npos;
    }

private:
    std::array<std::uint64_t, NumWords> m_words { };
    std::size_t m_numWords;
};

constexpr bool startsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

constexpr bool setContains(std::string_view members, char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    std::size_t i = 0;
    while (i < members.size())
    {
        const auto low = static_cast<unsigned char>(members[i]);
        const bool isRange = i + 2 < members.size() && members[i + 1] == k::rangeSeparator;
        if (isRange)
        {
            const auto high = static_cast<unsigned char>(members[i + 2]);
            if (low <= value && value <= high)
            {
                return true;
            }
            i += 3;
        }
        else
        {
            if (low == value)
            {
                return true;
            }
            i += 1;
        }
    }
    return false;
}

template <std::size_t NumWords, typename Predicate>
constexpr Reach<NumWords> advanceWhere(const Reach<NumWords>& reach, std::string_view part, Predicate matches) noexcept
{
    Reach<NumWords> next(part.size());
    for (std::size_t position = 0; position < part.size(); ++position)
    {
        if (reach.test(position) && matches(part[position]))
        {
            next.set(position + 1);
        }
    }
    return next;
}

template <std::size_t NumWords>
constexpr Reach<NumWords> advanceByAlternatives(const Reach<NumWords>& reach, std::string_view part, std::string_view list) noexcept
{
    Reach<NumWords> next(part.size());
    std::string_view remaining = list;
    bool moreAlternatives = true;
    while (moreAlternatives)
    {
        const std::size_t comma = remaining.find(k::listSeparator);
        const std::string_view alternative = remaining.substr(0, comma);
        for (std::size_t position = 0; position <= part.size(); ++position)
        {
            if (reach.test(position) && startsWith(part.substr(position), alternative))
            {
                next.set(position + alternative.size());
            }
        }
        moreAlternatives = comma != npos;
        if (moreAlternatives)
        {
            remaining = remaining.substr(comma + 1);
        }
    }
    return next;
}

constexpr bool isOpener(char byte) noexcept
{
    return byte == k::anyBytes || byte == k::anyByte || byte == k::setOpen || byte == k::listOpen;
}

constexpr bool hasOpener(std::string_view text) noexcept
{
    for (const char byte : text)
    {
        if (isOpener(byte))
        {
            return true;
        }
    }
    return false;
}

template <std::size_t NumWords>
constexpr bool matchPartWithin(std::string_view pattern, std::string_view part) noexcept
{
    Reach<NumWords> reach(part.size());
    reach.set(0);
    std::size_t i = 0;
    while (i < pattern.size() && !reach.empty())
    {
        const char byte = pattern[i];
        if (byte == k::anyBytes)
        {
            reach.setFrom(reach.lowest(), part.size());
            ++i;
        }
        else if (byte == k::anyByte)
        {
            reach = advanceWhere(reach, part, []([[maybe_unused]] char candidate)
                { return true; });
            ++i;
        }
        else if (byte == k::setOpen)
        {
            std::size_t start = i + 1;
            const bool negate = start < pattern.size() && pattern[start] == k::setNegate;
            if (negate)
            {
                ++start;
            }
            const std::size_t close = pattern.find(k::setClose, start);
            if (close == npos)
            {
                return false;
            }
            const std::string_view members = pattern.substr(start, close - start);
            reach = advanceWhere(reach, part, [members, negate](char candidate)
                { return setContains(members, candidate) != negate; });
            i = close + 1;
        }
        else if (byte == k::listOpen)
        {
            const std::size_t close = pattern.find(k::listClose, i + 1);
            if (close == npos)
            {
                return false;
            }
            reach = advanceByAlternatives(reach, part, pattern.substr(i + 1, close - i - 1));
            i = close + 1;
        }
        else
        {
            reach = advanceWhere(reach, part, [byte](char candidate)
                { return candidate == byte; });
            ++i;
        }
    }
    return reach.test(part.size());
}

constexpr bool matchPart(std::string_view pattern, std::string_view part) noexcept
{
    if (part.size() > kMaxAddressPartLength)
    {
        return false;
    }
    if (!hasOpener(pattern))
    {
        return pattern == part;
    }
    if (part.size() < k::bitsPerWord)
    {
        return matchPartWithin<k::smallReachWords>(pattern, part);
    }
    return matchPartWithin<k::reachWords>(pattern, part);
}

class PatternCursor
{
public:
    constexpr explicit PatternCursor(std::string_view text) noexcept
        : m_text(text)
    {
        readToken();
    }

    constexpr bool exhausted() const noexcept
    {
        return m_exhausted;
    }

    constexpr bool isOperator() const noexcept
    {
        return m_isOperator;
    }

    constexpr std::string_view part() const noexcept
    {
        return m_text.substr(m_partStart, m_partEnd - m_partStart);
    }

    constexpr bool matches(std::string_view addressPart) const noexcept
    {
        return matchPart(part(), addressPart);
    }

    constexpr void advance() noexcept
    {
        m_position = m_next;
        readToken();
    }

private:
    constexpr void readToken() noexcept
    {
        if (m_position >= m_text.size())
        {
            m_exhausted = true;
            return;
        }
        m_partStart = m_position;
        if (m_text[m_position] == k::partSeparator)
        {
            std::size_t runLength = 0;
            while (m_position + runLength < m_text.size() && m_text[m_position + runLength] == k::partSeparator)
            {
                ++runLength;
            }
            if (runLength >= k::operatorRunLength)
            {
                m_isOperator = true;
                m_next = m_position + runLength;
                return;
            }
            m_partStart = m_position + 1;
        }
        m_isOperator = false;
        const std::size_t separator = m_text.find(k::partSeparator, m_partStart);
        m_partEnd = separator == npos ? m_text.size() : separator;
        m_next = m_partEnd;
    }

    std::string_view m_text;
    std::size_t m_position = 0;
    std::size_t m_next = 0;
    std::size_t m_partStart = 0;
    std::size_t m_partEnd = 0;
    bool m_isOperator = false;
    bool m_exhausted = false;
};

class AddressCursor
{
public:
    constexpr explicit AddressCursor(std::string_view text) noexcept
        : m_text(text)
    {
        readPart();
    }

    constexpr bool exhausted() const noexcept
    {
        return m_partStart > m_text.size();
    }

    constexpr std::string_view part() const noexcept
    {
        return m_text.substr(m_partStart, m_partEnd - m_partStart);
    }

    constexpr void advance() noexcept
    {
        m_partStart = m_partEnd + 1;
        readPart();
    }

private:
    constexpr void readPart() noexcept
    {
        if (exhausted())
        {
            return;
        }
        const std::size_t separator = m_text.find(k::partSeparator, m_partStart);
        m_partEnd = separator == npos ? m_text.size() : separator;
    }

    std::string_view m_text;
    std::size_t m_partStart = 1;
    std::size_t m_partEnd = 0;
};

constexpr std::size_t closeWithinPart(std::string_view pattern, std::size_t open, char closeByte) noexcept
{
    const std::size_t close = pattern.find(closeByte, open + 1);
    if (close == npos || pattern.substr(open + 1, close - open - 1).find(k::partSeparator) != npos)
    {
        return npos;
    }
    return close;
}

constexpr bool isPrintableAscii(char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    return value >= k::firstPrintableAscii && value <= k::lastPrintableAscii;
}

constexpr bool isReservedInAddress(char byte) noexcept
{
    return k::reservedInAddress.find(byte) != npos;
}

constexpr bool hasLeadingSlash(std::string_view text) noexcept
{
    return !text.empty() && text[0] == k::partSeparator;
}

constexpr bool hasWildcard(std::string_view pattern) noexcept
{
    return hasOpener(pattern) || pattern.find(k::descendantOperator) != npos;
}

constexpr bool isLiteralText(std::string_view pattern) noexcept
{
    std::size_t partLength = 0;
    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        const char byte = pattern[i];
        if (isOpener(byte))
        {
            return false;
        }
        if (byte == k::partSeparator)
        {
            if (i + 1 < pattern.size() && pattern[i + 1] == k::partSeparator)
            {
                return false;
            }
            partLength = 0;
        }
        else if (++partLength > kMaxAddressPartLength)
        {
            return false;
        }
    }
    return true;
}

template <typename PatternParts, typename AddressParts>
constexpr bool matchParts(PatternParts patternPart, AddressParts addressPart) noexcept
{
    PatternParts patternAfterOperator = patternPart;
    AddressParts addressAtOperator = addressPart;
    bool seenOperator = false;
    while (!patternPart.exhausted() || !addressPart.exhausted())
    {
        if (!patternPart.exhausted() && patternPart.isOperator())
        {
            patternPart.advance();
            patternAfterOperator = patternPart;
            addressAtOperator = addressPart;
            seenOperator = true;
            continue;
        }
        if (!patternPart.exhausted() && !addressPart.exhausted() && patternPart.matches(addressPart.part()))
        {
            patternPart.advance();
            addressPart.advance();
            continue;
        }
        if (!seenOperator || addressAtOperator.exhausted())
        {
            return false;
        }
        addressAtOperator.advance();
        patternPart = patternAfterOperator;
        addressPart = addressAtOperator;
    }
    return true;
}

constexpr bool matchParsed(std::string_view pattern, std::string_view address) noexcept
{
    return hasLeadingSlash(address) && matchParts(PatternCursor(pattern), AddressCursor(address));
}

}

namespace oscpm
{

/// The first fault in `pattern` by byte offset, or nothing when it parses:
/// `MissingLeadingSlash` at 0, `UnterminatedClass` at a '[' with no ']'
/// before the next '/', `UnterminatedBraces` at a '{' with no '}' before the
/// next '/', `PatternTooLong` at `kMaxPatternLength` for a longer pattern
/// containing '*', '?', '[', '{' or "//".
/// Every other pattern parses; a byte no address can contain is a literal
/// that matches nothing where it stands.
constexpr std::optional<ParseError> validatePattern(std::string_view pattern) noexcept
{
    if (!detail::hasLeadingSlash(pattern))
    {
        return ParseError { Error::MissingLeadingSlash, 0 };
    }
    const std::size_t checked = pattern.size() < kMaxPatternLength ? pattern.size() : kMaxPatternLength;
    std::size_t i = 0;
    while (i < checked)
    {
        if (pattern[i] == detail::k::setOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::setClose);
            if (close == detail::npos)
            {
                return ParseError { Error::UnterminatedClass, i };
            }
            i = close + 1;
        }
        else if (pattern[i] == detail::k::listOpen)
        {
            const std::size_t close = detail::closeWithinPart(pattern, i, detail::k::listClose);
            if (close == detail::npos)
            {
                return ParseError { Error::UnterminatedBraces, i };
            }
            i = close + 1;
        }
        else
        {
            ++i;
        }
    }
    if (pattern.size() > kMaxPatternLength && detail::hasWildcard(pattern))
    {
        return ParseError { Error::PatternTooLong, kMaxPatternLength };
    }
    return std::nullopt;
}

/// The first fault in `address` by byte offset, or nothing when it is a
/// well-formed OSC address: `MissingLeadingSlash` at 0, `TrailingSlash` at
/// a final '/' (the bare "/" faults at 0), `EmptyPart` at the second of two
/// adjacent slashes, `IllegalByte` at a byte outside printable ASCII or one
/// of " #*,?[]{}", `PartTooLong` at the first byte of a part beyond
/// `kMaxAddressPartLength`.
constexpr std::optional<ParseError> validateAddress(std::string_view address) noexcept
{
    if (!detail::hasLeadingSlash(address))
    {
        return ParseError { Error::MissingLeadingSlash, 0 };
    }
    std::size_t partLength = 0;
    for (std::size_t i = 1; i <= address.size(); ++i)
    {
        const bool atEnd = i == address.size();
        if (atEnd || address[i] == detail::k::partSeparator)
        {
            if (partLength == 0)
            {
                return atEnd ? ParseError { Error::TrailingSlash, i - 1 } : ParseError { Error::EmptyPart, i };
            }
            partLength = 0;
        }
        else if (!detail::isPrintableAscii(address[i]) || detail::isReservedInAddress(address[i]))
        {
            return ParseError { Error::IllegalByte, i };
        }
        else if (++partLength > kMaxAddressPartLength)
        {
            return ParseError { Error::PartTooLong, i };
        }
    }
    return std::nullopt;
}

class ParseResult;

/// A validated address pattern. Holds a view of the caller's bytes, which
/// must outlive every use; copying the value copies the view.
class Pattern
{
public:
    /// Parses `text`, yielding the pattern or the first fault by byte offset
    /// as `validatePattern` reports it.
    static constexpr ParseResult parse(std::string_view text) noexcept;

    /// Whether this pattern matches `address`: the OSC 1.0 rules for '?',
    /// '*', '[...]' and '{a,b}', applied part by part, plus the OSC 1.1 '//'
    /// operator matching zero or more whole parts, which a run of two or
    /// more slashes anywhere in the pattern denotes. The address is compared
    /// byte for byte and never validated. Allocation-free, with running time
    /// bounded by the product of the two lengths.
    constexpr bool matches(std::string_view address) const noexcept
    {
        return m_isLiteral ? address == m_text : detail::matchParsed(m_text, address);
    }

    /// The bytes this pattern was parsed from.
    constexpr std::string_view text() const noexcept
    {
        return m_text;
    }

    /// Whether the pattern contains no '*', '?', '[' or '{', no run of two
    /// or more slashes and no part longer than `kMaxAddressPartLength`, so
    /// that it matches only an address equal to its text.
    constexpr bool isLiteral() const noexcept
    {
        return m_isLiteral;
    }

private:
    friend class ParseResult;

    constexpr Pattern() noexcept = default;

    constexpr explicit Pattern(std::string_view text) noexcept
        : m_text(text)
        , m_isLiteral(detail::isLiteralText(text))
    {
    }

    std::string_view m_text;
    bool m_isLiteral = false;
};

/// A `Pattern` or the `ParseError` that stopped it parsing.
class ParseResult
{
public:
    /// Whether parsing succeeded and `pattern()` holds the result.
    constexpr explicit operator bool() const noexcept
    {
        return m_parsed;
    }

    /// The pattern; meaningful only when the result is true.
    constexpr const Pattern& pattern() const noexcept
    {
        return m_pattern;
    }

    /// The fault; meaningful only when the result is false.
    constexpr const ParseError& error() const noexcept
    {
        return m_error;
    }

private:
    friend class Pattern;

    constexpr explicit ParseResult(Pattern pattern) noexcept
        : m_pattern(pattern)
        , m_parsed(true)
    {
    }

    constexpr explicit ParseResult(ParseError error) noexcept
        : m_error(error)
    {
    }

    Pattern m_pattern;
    ParseError m_error { Error::MissingLeadingSlash, 0 };
    bool m_parsed = false;
};

constexpr ParseResult Pattern::parse(std::string_view text) noexcept
{
    const std::optional<ParseError> fault = validatePattern(text);
    return fault.has_value() ? ParseResult(*fault) : ParseResult(Pattern(text));
}

/// Parses `pattern` and tests it against `address`, as `Pattern::parse`
/// followed by `Pattern::matches`. A malformed pattern matches nothing; use
/// `Pattern::parse` to learn why.
constexpr bool match(std::string_view pattern, std::string_view address) noexcept
{
    const ParseResult parsed = Pattern::parse(pattern);
    return parsed && parsed.pattern().matches(address);
}

}
