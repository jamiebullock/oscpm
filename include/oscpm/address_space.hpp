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

#include <oscpm/pattern.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oscpm {

// A Method as the span-filling Lookup reports it: its Address and a pointer
// to its value. Both stay valid until the Method is removed or the Address
// Space is destroyed (ADR 0005). `T` is const in the const overload.
template <typename T>
struct Method {
    std::string_view address;
    T* value;
};

// What a span-filling Lookup did: how many Methods it wrote to the caller's
// storage, and how many the Pattern Matched. The second exceeds the first
// only when the storage was too small, which is not an error.
struct LookupCount {
    std::size_t stored;
    std::size_t matched;
};

// The tree of Containers and Methods that Patterns are matched against.
// Each Method holds a caller-supplied value of type T, which need only be
// move-constructible: std::unique_ptr and capturing lambdas both work.
//
// Registration (`add`, `remove`) is meant for the setup thread: `add` may
// allocate, and so may propagate std::bad_alloc from the standard
// containers it grows. `lookup` and `forEach` never allocate and throw
// nothing of their own, so they can run inside an audio callback. Mutating
// the Address Space while another thread runs a Lookup is not supported.
//
// The `T&` a visitor receives, and the Address it is given, stay valid until
// the Method is removed or the Address Space is destroyed; other
// registrations do not move them (ADR 0005).
template <typename T>
class AddressSpace {
public:
    AddressSpace() = default;
    AddressSpace(AddressSpace&&) noexcept = default;
    AddressSpace& operator=(AddressSpace&&) noexcept = default;

    // Registers a Method at a literal Address, moving `value` in. Reports
    // Malformed with the offending byte's offset, or Duplicate if a Method
    // is already registered there, in which case `value` is destroyed. An
    // Address may be both a Container and a Method: "/synth" and
    // "/synth/freq" coexist.
    std::optional<Error> add(std::string_view address, T value)
    {
        if (const std::optional<Error> error = validateAddress(address)) {
            return error;
        }
        Children* children = &m_children;
        Entry* entry = nullptr;
        for (Parts parts(address);; parts.advance()) {
            const auto it = lowerBound(*children, parts.part());
            if (it == children->end() || (*it)->part() != parts.part()) {
                entry = children->insert(it, std::make_unique<Entry>(parts))->get();
            } else {
                entry = it->get();
            }
            if (parts.last()) {
                break;
            }
            children = &entry->children;
        }
        if (entry->value) {
            return Error{ErrorKind::Duplicate, 0};
        }
        entry->value.emplace(std::move(value));
        return std::nullopt;
    }

    // Unregisters the Method at `address`, destroying its value. Reports
    // Malformed for a Malformed Address and NotFound when no Method is
    // registered there. A Container that is also a Method keeps its
    // children; a Container left with neither Method nor children is
    // released.
    std::optional<Error> remove(std::string_view address)
    {
        if (const std::optional<Error> error = validateAddress(address)) {
            return error;
        }
        Entry* entry = find(address);
        if (entry == nullptr || !entry->value) {
            return Error{ErrorKind::NotFound, 0};
        }
        entry->value.reset();
        prune(address);
        return std::nullopt;
    }

    // Calls `visitor(std::string_view address, T& value)` for every Method
    // in address order: Part by Part, each Part compared bytewise, a Method
    // before the Methods beneath it. Never allocates. Recursion depth is the
    // Part count of the deepest registered Address, which the caller
    // controls.
    template <typename Visitor>
    void forEach(Visitor&& visitor)
    {
        forEachIn<T>(m_children, visitor);
    }

    // As above, calling `visitor(std::string_view address, const T& value)`.
    template <typename Visitor>
    void forEach(Visitor&& visitor) const
    {
        forEachIn<const T>(m_children, visitor);
    }

    // Finds every Method whose Address `pattern` Matches and calls
    // `visitor(std::string_view address, T& value)` once for each, in
    // address order. Returns the number of Methods visited. A Pattern with
    // no Wildcards costs one binary search per Part; any other walks the
    // tree, entering a Container only while the Pattern could still Match
    // something beneath it. Never allocates. Recursion depth is as for
    // forEach.
    template <typename Visitor>
    std::size_t lookup(Pattern pattern, Visitor&& visitor)
    {
        return lookupWith<T>(pattern, visitor);
    }

    // As above, calling `visitor(std::string_view address, const T& value)`.
    template <typename Visitor>
    std::size_t lookup(Pattern pattern, Visitor&& visitor) const
    {
        return lookupWith<const T>(pattern, visitor);
    }

    // As the visitor form, but writes the matching Methods in address order
    // to `out`, which has room for `capacity` of them. Once `out` is full
    // the remaining Methods are counted but not written, so the result's
    // `matched` may exceed its `stored`. Never allocates.
    LookupCount lookup(Pattern pattern, Method<T>* out, std::size_t capacity)
    {
        return fill<T>(pattern, out, capacity);
    }

    // As above, writing pointers to const.
    LookupCount lookup(Pattern pattern, Method<const T>* out, std::size_t capacity) const
    {
        return fill<const T>(pattern, out, capacity);
    }

private:
    // Walks the Parts of a well-formed Address, first to last.
    class Parts {
    public:
        explicit Parts(std::string_view address) noexcept
            : m_address(address), m_start(1), m_end(detail::partEnd(address, 1))
        {
        }

        std::string_view part() const noexcept
        {
            return m_address.substr(m_start, m_end - m_start);
        }
        // The Address up to and including the current Part.
        std::string_view prefix() const noexcept { return m_address.substr(0, m_end); }
        std::size_t partStart() const noexcept { return m_start; }
        bool last() const noexcept { return m_end == m_address.size(); }

        // Precondition: !last().
        void advance() noexcept
        {
            m_start = m_end + 1;
            m_end = detail::partEnd(m_address, m_start);
        }

    private:
        std::string_view m_address;
        std::size_t m_start;
        std::size_t m_end;
    };

    // One Part of the tree, with its children. An Entry is a Container
    // when it has children and a Method when it holds a value; it may be
    // both. Entries are heap allocated once and never move, which is what
    // keeps the references a visitor receives stable across later
    // registrations (ADR 0005).
    struct Entry {
        explicit Entry(const Parts& parts) : address(parts.prefix()), partStart(parts.partStart())
        {
        }

        // The Part this Entry is named by: the last Part of `address`.
        std::string_view part() const noexcept
        {
            return std::string_view(address).substr(partStart);
        }

        std::string address; // The full Address of this Entry.
        std::size_t partStart;
        std::vector<std::unique_ptr<Entry>> children; // Sorted by part().
        std::optional<T> value;
    };

    using Children = std::vector<std::unique_ptr<Entry>>;

    // The first child whose Part is not less than `wanted`, comparing bytes.
    template <typename C>
    static auto lowerBound(C& children, std::string_view wanted) noexcept
    {
        return std::lower_bound(children.begin(), children.end(), wanted,
                                [](const std::unique_ptr<Entry>& entry, std::string_view part) {
                                    return entry->part() < part;
                                });
    }

    // The child named `wanted`, or nullptr. Children are owned through
    // unique_ptr, so a const walk still yields a mutable Entry.
    static Entry* findChild(const Children& children, std::string_view wanted) noexcept
    {
        const auto it = lowerBound(children, wanted);
        return it != children.end() && (*it)->part() == wanted ? it->get() : nullptr;
    }

    // Walks `address` one binary search per Part to the Entry it names, or
    // nullptr if no such Entry exists.
    Entry* find(std::string_view address) const noexcept
    {
        const Children* children = &m_children;
        for (Parts parts(address);; parts.advance()) {
            Entry* entry = findChild(*children, parts.part());
            if (entry == nullptr || parts.last()) {
                return entry;
            }
            children = &entry->children;
        }
    }

    // Releases the Entries along `address` that no longer serve any Method:
    // the topmost Entry on the path that is neither a Method nor a
    // Container of anything off the path, and everything beneath it. One
    // walk from the root finds that Entry, so no path needs to be recorded.
    // Precondition: every Part of `address` is in the tree.
    void prune(std::string_view address) noexcept
    {
        Children* children = &m_children;
        Children* cutChildren = nullptr; // Where the topmost unneeded Entry lives.
        typename Children::iterator cut;
        for (Parts parts(address);; parts.advance()) {
            const auto it = lowerBound(*children, parts.part());
            Entry& entry = **it;
            const bool needed =
                entry.value || entry.children.size() > (parts.last() ? 0u : 1u);
            if (needed) {
                cutChildren = nullptr;
            } else if (cutChildren == nullptr) {
                cutChildren = children;
                cut = it;
            }
            if (parts.last()) {
                break;
            }
            children = &entry.children;
        }
        if (cutChildren != nullptr) {
            cutChildren->erase(cut);
        }
    }

    // Delivers the Method at `entry` to `visitor`. `Value` is T or const T,
    // so one helper serves the const and non-const overloads; a const walk
    // still reaches a mutable Entry through its unique_ptr. Precondition:
    // `entry` holds a value.
    template <typename Value, typename Visitor>
    static void visit(Entry& entry, Visitor& visitor)
    {
        visitor(std::string_view(entry.address), static_cast<Value&>(*entry.value));
    }

    // Depth first over `children`, each Entry's own Method before its
    // children's.
    template <typename Value, typename Visitor>
    static void forEachIn(const Children& children, Visitor& visitor)
    {
        for (const std::unique_ptr<Entry>& entry : children) {
            if (entry->value) {
                visit<Value>(*entry, visitor);
            }
            forEachIn<Value>(entry->children, visitor);
        }
    }

    // A Pattern with no Wildcard and no Descendant Operator names exactly
    // one Address.
    static bool isLiteral(std::string_view pattern) noexcept
    {
        for (std::size_t i = 1; i < pattern.size(); ++i) {
            const char c = pattern[i];
            if (c == '?' || c == '*' || c == '[' || c == '{'
                || (c == '/' && pattern[i - 1] == '/')) {
                return false;
            }
        }
        return true;
    }

    // Both `lookup` overloads: the literal fast path or the tree walk.
    template <typename Value, typename Visitor>
    std::size_t lookupWith(Pattern pattern, Visitor& visitor) const
    {
        const std::string_view text = pattern.text();
        if (isLiteral(text)) {
            Entry* entry = find(text);
            if (entry != nullptr && entry->value) {
                visit<Value>(*entry, visitor);
                return 1;
            }
            return 0;
        }
        return lookupIn<Value>(m_children, text, detail::MatchState{}, visitor);
    }

    // The tree walk behind a wildcard Lookup: depth first over `children`
    // in address order, so each Method is seen once and a Method before its
    // children, as forEach. `state` is the matcher's position after the
    // Parts above `children`. Each child advances a copy of it over its own
    // Part; after a "//" that can mean resuming from an earlier Part of the
    // Address, exactly as matching the child's whole Address would. A child
    // is skipped along with everything beneath it once no Address under it
    // could Match.
    template <typename Value, typename Visitor>
    static std::size_t lookupIn(const Children& children, std::string_view pattern,
                                const detail::MatchState& state, Visitor& visitor)
    {
        std::size_t count = 0;
        for (const std::unique_ptr<Entry>& entry : children) {
            detail::MatchState next = state;
            if (!detail::matchParts(next, pattern, entry->address, entry->partStart)) {
                continue;
            }
            if (next.complete(pattern)) {
                if (entry->value) {
                    visit<Value>(*entry, visitor);
                    ++count;
                }
                if (!next.canAbsorbMore()) {
                    continue;
                }
            }
            count += lookupIn<Value>(entry->children, pattern, next, visitor);
        }
        return count;
    }

    // Both span-filling overloads, built on the visitor form.
    template <typename Value>
    LookupCount fill(Pattern pattern, Method<Value>* out, std::size_t capacity) const
    {
        LookupCount count{0, 0};
        const auto store = [&](std::string_view address, Value& value) {
            if (count.stored < capacity) {
                out[count.stored++] = Method<Value>{address, &value};
            }
        };
        count.matched = lookupWith<Value>(pattern, store);
        return count;
    }

    Children m_children; // The root's children, sorted by part().
};

} // namespace oscpm
