// oscpm - OSC address pattern matching for C++
//
// Copyright (c) 2026 Jamie Bullock
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "pattern.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace oscpm {

/// A set of literal OSC addresses, each with an associated value, that an address pattern can be
/// dispatched against.
///
/// Typical use is `AddressSpace<std::function<void(Args...)>>` or `AddressSpace<Method*>`, where
/// the value is whatever should run when a message matches. Entries are kept sorted by address, so
/// dispatch visits matches in lexicographic order and a pattern's literal prefix narrows the search.
template <class T>
class AddressSpace
{
    using container = std::map<std::string, T, std::less<>>;

public:
    using value_type = typename container::value_type;
    using mapped_type = T;
    using const_iterator = typename container::const_iterator;
    using iterator = typename container::iterator;

    /// Registers `value` at the literal `address`, replacing any existing entry. Returns the
    /// validation error if `address` is not a well formed literal address, in which case nothing
    /// is changed.
    Error add(std::string_view address, T value)
    {
        if (const Error error = validate_address(address))
            return error;
        m_entries.insert_or_assign(std::string(address), std::move(value));
        return {};
    }

    /// Removes the entry at `address`. Returns false if there was none.
    bool remove(std::string_view address)
    {
        const auto it = m_entries.find(address);
        if (it == m_entries.end())
            return false;
        m_entries.erase(it);
        return true;
    }

    /// Looks up the value registered at exactly `address`.
    T* find(std::string_view address) noexcept
    {
        const auto it = m_entries.find(address);
        return it == m_entries.end() ? nullptr : &it->second;
    }

    const T* find(std::string_view address) const noexcept
    {
        const auto it = m_entries.find(address);
        return it == m_entries.end() ? nullptr : &it->second;
    }

    bool contains(std::string_view address) const noexcept { return m_entries.find(address) != m_entries.end(); }

    std::size_t size() const noexcept { return m_entries.size(); }
    bool empty() const noexcept { return m_entries.empty(); }
    void clear() noexcept { m_entries.clear(); }

    iterator begin() noexcept { return m_entries.begin(); }
    iterator end() noexcept { return m_entries.end(); }
    const_iterator begin() const noexcept { return m_entries.begin(); }
    const_iterator end() const noexcept { return m_entries.end(); }

    /// Calls `f(const std::string& address, T& value)` for every entry whose address matches
    /// `pattern`, in address order, and returns how many were called. `f` must not add or remove
    /// entries. A malformed pattern matches nothing it should not; use `Pattern` to validate.
    template <class F>
    std::size_t dispatch(std::string_view pattern, F&& f)
    {
        return dispatch_impl(*this, pattern, is_literal(pattern), literal_prefix(pattern), f);
    }

    template <class F>
    std::size_t dispatch(std::string_view pattern, F&& f) const
    {
        return dispatch_impl(*this, pattern, is_literal(pattern), literal_prefix(pattern), f);
    }

    template <class F>
    std::size_t dispatch(const Pattern& pattern, F&& f)
    {
        return dispatch_impl(*this, pattern.str(), pattern.is_literal(), pattern.literal_prefix(), f);
    }

    template <class F>
    std::size_t dispatch(const Pattern& pattern, F&& f) const
    {
        return dispatch_impl(*this, pattern.str(), pattern.is_literal(), pattern.literal_prefix(), f);
    }

private:
    template <class Self, class F>
    static std::size_t dispatch_impl(Self& self, std::string_view pattern, bool literal, std::string_view prefix, F& f)
    {
        if (literal) {
            const auto it = self.m_entries.find(pattern);
            if (it == self.m_entries.end())
                return 0;
            f(it->first, it->second);
            return 1;
        }

        std::size_t count = 0;
        for (auto it = self.m_entries.lower_bound(prefix); it != self.m_entries.end(); ++it) {
            const std::string_view address = it->first;
            if (address.substr(0, prefix.size()) != prefix)
                break;
            if (match(pattern, address)) {
                f(it->first, it->second);
                ++count;
            }
        }
        return count;
    }

    container m_entries;
};

} // namespace oscpm
