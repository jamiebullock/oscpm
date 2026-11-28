/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/address.h>
#include <oscpm/address_space.h>
#include <oscpm/pattern.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace oscpm_test
{

/// Numbers drawn from a run of bytes, one byte each; every draw is 0 once
/// the bytes are used up.
class Choices
{
public:
    Choices(const std::uint8_t* bytes, std::size_t numBytes) noexcept
        : m_bytes(bytes)
        , m_numBytes(numBytes)
    {
    }

    bool exhausted() const noexcept
    {
        return m_position == m_numBytes;
    }

    /// A number below `limit`, which is at most 256.
    std::size_t below(std::size_t limit) noexcept
    {
        return exhausted() ? 0 : m_bytes[m_position++] % limit;
    }

private:
    const std::uint8_t* m_bytes;
    std::size_t m_numBytes;
    std::size_t m_position = 0;
};

struct Handler
{
    void operator()(std::vector<int>& called) const
    {
        called.push_back(number);
    }

    int number;
};

inline int& numberIn(int& value) noexcept
{
    return value;
}

template <typename T>
int& numberIn(T& value) noexcept
{
    return value.number;
}

template <typename T>
int numberOf(const T& value) noexcept
{
    return numberIn(const_cast<T&>(value));
}

using Entries = std::vector<std::pair<std::string, int>>;
using Model = std::map<std::string, int>;

struct Visit
{
    Entries entries;
    std::size_t matched = 0;
    std::optional<oscpm::PatternError> error;
};

inline bool operator==(const Visit& left, const Visit& right)
{
    return left.entries == right.entries && left.matched == right.matched && left.error == right.error;
}

inline Visit expectedVisit(const Model& model, std::string_view pattern)
{
    Visit visit;
    const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> parsed = oscpm::Pattern::parse(pattern);
    if (!parsed)
    {
        visit.error = parsed.error();
        return visit;
    }
    for (const auto& [address, number] : model)
    {
        if (parsed->matches(oscpm::Address(address)))
        {
            visit.entries.emplace_back(address, number);
        }
    }
    visit.matched = visit.entries.size();
    return visit;
}

template <typename Space>
Visit visitOf(Space& space, std::string_view pattern)
{
    Visit visit;
    const oscpm::MatchResult result = space.visit(pattern, [&](std::string_view address, const auto& value)
        { visit.entries.emplace_back(std::string(address), numberOf(value)); });
    visit.matched = result.matched;
    visit.error = result.error;
    return visit;
}

inline std::string repeated(std::string_view text, std::size_t count)
{
    std::string result;
    for (std::size_t i = 0; i < count; ++i)
    {
        result += text;
    }
    return result;
}

/// Applies the operations that `choices` selects to a `std::map` and to
/// address spaces with and without a memo, with small memos and with
/// handlers.
class ModelRun
{
public:
    explicit ModelRun(Choices& choices)
        : m_choices(choices)
    {
    }

    std::string firstDifference()
    {
        while (!m_choices.exhausted() && m_difference.empty())
        {
            ++m_numOperations;
            operation();
            if (m_numOperations % k::operationsBetweenFullChecks == 0)
            {
                checkContents();
            }
        }
        checkContents();
        return m_difference;
    }

private:
    struct k
    {
        static constexpr std::size_t operationsBetweenFullChecks = 16;
        static constexpr std::size_t preparedParts = 64;
        static constexpr std::size_t eitherSide = 8;
    };

    using Spaces = std::tuple<
        oscpm::AddressSpace<int>,
        oscpm::AddressSpace<int, false>,
        oscpm::AddressSpace<int, true, 0, 4>,
        oscpm::AddressSpace<int, true, 2, 2>,
        oscpm::AddressSpace<Handler, true, 2, 1>,
        oscpm::AddressSpace<Handler, false>,
        oscpm::AddressSpace<Handler, true, 1, 3>>;

    template <typename Function>
    void forEachSpace(Function&& function)
    {
        std::size_t index = 0;
        std::apply([&](auto&... space)
            { (function(space, index++), ...); }, m_spaces);
    }

    void expect(bool same, std::size_t index, const std::string& operation)
    {
        if (!same && m_difference.empty())
        {
            m_difference = "operation " + std::to_string(m_numOperations) + ", " + operation + ", differs in space " + std::to_string(index);
        }
    }

    void operation()
    {
        switch (m_choices.below(12))
        {
        case 0:
        case 1:
        case 2:
            add();
            break;
        case 3:
        case 4:
        case 5:
            remove();
            break;
        case 6:
            find();
            break;
        case 7:
        case 8:
            visit();
            break;
        case 9:
            dispatch();
            break;
        case 10:
            changeValuesAndVisitWithin();
            break;
        default:
            copyOrMove();
            break;
        }
    }

    std::string address()
    {
        static constexpr const char* kParts[] = { "a", "b", "ab", "1", "12" };
        const std::size_t kind = m_choices.below(32);
        if (kind == 0)
        {
            return repeated("/a", k::preparedParts - k::eitherSide / 2 + m_choices.below(k::eitherSide));
        }
        if (kind == 1)
        {
            return "/" + std::string(oscpm::kMaxMemoPatternLength - k::eitherSide + m_choices.below(k::eitherSide), 'a');
        }
        std::string text;
        const std::size_t numParts = 1 + m_choices.below(3);
        for (std::size_t i = 0; i < numParts; ++i)
        {
            text += "/";
            text += kParts[m_choices.below(std::size(kParts))];
        }
        return text;
    }

    std::string registeredOrOtherAddress()
    {
        if (m_model.empty() || m_choices.below(4) == 0)
        {
            return address();
        }
        const std::size_t position = m_choices.below(std::min<std::size_t>(m_model.size(), 256));
        return std::next(m_model.begin(), static_cast<std::ptrdiff_t>(position))->first;
    }

    std::string pattern()
    {
        static constexpr const char* kWhole[] = { "/", "//", "", "a", "/a/", "/[a", "/{a", "/a//", "/\x7f", "/[!a]*//b" };
        static constexpr const char* kParts[] = { "a", "b", "ab", "1", "12", "*", "?", "a*", "*1", "?b", "*a*", "[ab]", "[!a]", "[1-2]", "[]", "{a,b}", "{ab,12}", "{a,}", "]", "}" };
        switch (m_choices.below(16))
        {
        case 0:
            return kWhole[m_choices.below(std::size(kWhole))];
        case 1:
            return registeredOrOtherAddress();
        case 2:
            return repeated("/*", k::preparedParts - k::eitherSide / 2 + m_choices.below(k::eitherSide));
        case 3:
            return "/*" + std::string(oscpm::kMaxMemoPatternLength - k::eitherSide + m_choices.below(k::eitherSide), 'a');
        case 4:
            return "/*" + std::string(oscpm::kMaxPatternLength, 'a');
        default:
            break;
        }
        std::string text;
        const std::size_t numParts = 1 + m_choices.below(3);
        for (std::size_t i = 0; i < numParts; ++i)
        {
            text += m_choices.below(4) == 0 ? "//" : "/";
            text += kParts[m_choices.below(std::size(kParts))];
        }
        return text;
    }

    void add()
    {
        const std::string text = address();
        const int number = m_nextNumber++;
        const bool added = m_model.emplace(text, number).second;
        forEachSpace([&](auto& space, std::size_t index)
            { expect(space.add(oscpm::Address(text), valueOf(space, number)) == added && space.size() == m_model.size(), index, "add of " + text); });
        visitRecentPatternAgain();
    }

    void remove()
    {
        const std::string text = registeredOrOtherAddress();
        const bool removed = m_model.erase(text) != 0;
        forEachSpace([&](auto& space, std::size_t index)
            { expect(space.remove(oscpm::Address(text)) == removed && space.size() == m_model.size(), index, "remove of " + text); });
        visitRecentPatternAgain();
    }

    void find()
    {
        const std::string text = m_choices.below(4) == 0 ? pattern() : registeredOrOtherAddress();
        const auto entry = m_model.find(text);
        const std::optional<int> expected = entry == m_model.end() ? std::nullopt : std::optional<int>(entry->second);
        forEachSpace([&](auto& space, std::size_t index)
            { expect(found(space, text) == expected && found(std::as_const(space), text) == expected, index, "find of " + text); });
    }

    void visit()
    {
        m_recentPattern = pattern();
        const Visit expected = expectedVisit(m_model, m_recentPattern);
        forEachSpace([&](auto& space, std::size_t index)
            { expect(visitOf(space, m_recentPattern) == expected && visitOf(std::as_const(space), m_recentPattern) == expected, index, "visit of " + m_recentPattern); });
    }

    void visitRecentPatternAgain()
    {
        const Visit expected = expectedVisit(m_model, m_recentPattern);
        forEachSpace([&](auto& space, std::size_t index)
            { expect(visitOf(space, m_recentPattern) == expected, index, "visit of " + m_recentPattern + " after an add or remove"); });
    }

    void dispatch()
    {
        const std::string text = pattern();
        const Visit expected = expectedVisit(m_model, text);
        forEachSpace([&](auto& space, std::size_t index)
            { expect(dispatchAgrees(space, text, expected) && dispatchAgrees(std::as_const(space), text, expected), index, "dispatch of " + text); });
    }

    void changeValuesAndVisitWithin()
    {
        const std::string outer = m_choices.below(2) == 0 ? m_recentPattern : pattern();
        const std::string inner = pattern();
        const Visit expectedOuter = expectedVisit(m_model, outer);
        const Visit expectedInner = expectedVisit(m_model, inner);
        const int firstNumber = m_nextNumber;
        forEachSpace([&](auto& space, std::size_t index)
            { expect(visitWithinAgrees(space, outer, inner, firstNumber, expectedOuter, expectedInner), index, "visit of " + inner + " within a visit of " + outer); });
        for (const auto& entry : expectedOuter.entries)
        {
            m_model[entry.first] = m_nextNumber++;
        }
    }

    void copyOrMove()
    {
        const std::size_t kind = m_choices.below(3);
        forEachSpace([&](auto& space, std::size_t)
            { copyOrMove(space, kind); });
        checkContents();
    }

    void checkContents()
    {
        const Entries expected(m_model.begin(), m_model.end());
        forEachSpace([&](auto& space, std::size_t index)
            { expect(contentsOf(space) == expected && space.size() == expected.size(), index, "visit of every address"); });
    }

    template <typename T, bool Memo, unsigned CacheBits, std::size_t InlineResults>
    static T valueOf(const oscpm::AddressSpace<T, Memo, CacheBits, InlineResults>&, int number)
    {
        return T { number };
    }

    template <typename Space>
    static std::optional<int> found(Space& space, std::string_view text)
    {
        const auto* const value = space.find(text);
        return value == nullptr ? std::nullopt : std::optional<int>(numberOf(*value));
    }

    template <typename Space>
    static Entries contentsOf(const Space& space)
    {
        Entries entries;
        space.visit([&](std::string_view address, const auto& value)
            { entries.emplace_back(std::string(address), numberOf(value)); });
        return entries;
    }

    template <typename Space>
    static bool dispatchAgrees(Space& space, std::string_view text, const Visit& expected)
    {
        using Value = std::remove_reference_t<decltype(*space.find(text))>;
        if constexpr (std::is_invocable_v<Value&, std::vector<int>&>)
        {
            std::vector<int> called;
            const oscpm::MatchResult result = space.dispatch(text, called);
            std::vector<int> numbers;
            for (const auto& entry : expected.entries)
            {
                numbers.push_back(entry.second);
            }
            return called == numbers && result.matched == expected.matched && result.error == expected.error;
        }
        else
        {
            return true;
        }
    }

    /// Visits `outer`, giving each value it reaches the next number from
    /// `firstNumber`, and visits `inner` from inside the first call. Returns
    /// whether both visits reached what was expected.
    template <typename Space>
    static bool visitWithinAgrees(Space& space, std::string_view outer, std::string_view inner, int firstNumber, const Visit& expectedOuter, const Visit& expectedInner)
    {
        Visit visit;
        bool innerAgrees = true;
        const oscpm::MatchResult result = space.visit(outer, [&](std::string_view address, auto& value)
            {
                innerAgrees = innerAgrees && (!visit.entries.empty() || visitOf(space, inner) == expectedInner);
                visit.entries.emplace_back(std::string(address), numberOf(value));
                numberIn(value) = firstNumber + static_cast<int>(visit.entries.size()) - 1; });
        visit.matched = result.matched;
        visit.error = result.error;
        return innerAgrees && visit == expectedOuter;
    }

    template <typename Space>
    static void copyOrMove(Space& space, std::size_t kind)
    {
        if (kind == 0)
        {
            const Space copy(space);
            space = copy;
        }
        else if (kind == 1)
        {
            Space moved(std::move(space));
            space = std::move(moved);
        }
        else
        {
            Space other;
            other = space;
            space = std::move(other);
        }
    }

    Choices& m_choices;
    Model m_model;
    Spaces m_spaces;
    std::size_t m_numOperations = 0;
    // No two values share a number, so the numbers a dispatch collects identify the handlers it called.
    int m_nextNumber = 0;
    std::string m_recentPattern = "//";
    std::string m_difference;
};

/// Runs the operations `choices` selects on a `std::map` and on address
/// spaces of several kinds. Returns a description of the first operation
/// whose result in a space differs from the map's, or an empty string.
inline std::string firstDifference(Choices& choices)
{
    return ModelRun(choices).firstDifference();
}

}
