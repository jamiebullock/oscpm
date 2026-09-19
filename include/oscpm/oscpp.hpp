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

// The oscpp adapter: a Pattern or a Lookup straight from an oscpp server
// Message. This header is the only part of oscpm that includes oscpp; the
// matcher and the Address Space never do. It adds no matching logic: it
// wraps the Message's address in a string_view and calls the core (ADR
// 0001).

#pragma once

#include <oscpm/address_space.hpp>

#include <oscpp/server.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace oscpm {

// What a Lookup for a Message did: how many Methods were visited, or, when
// the Message's address pattern was Malformed, the Error, in which case the
// visitor was not called and `matched` is 0. A Malformed pattern is thus
// never mistaken for a Message that reaches no Method.
struct LookupResult {
    std::size_t matched;
    std::optional<Error> error;
};

// Parses the address pattern an oscpp server Message carries. oscpp has
// already checked that the address is NUL-terminated inside the packet, so
// this only measures it. The Pattern views the packet's bytes, exactly as
// the Message does, and is valid for as long as they are (ADR 0003). Never
// allocates or throws.
inline ParseResult parsePattern(const OSCPP::Server::Message& message) noexcept
{
    return Pattern::parse(std::string_view(message.address()));
}

// Runs a Lookup for an oscpp server Message: parses its address pattern
// and, when it is well-formed, calls `visitor(std::string_view address,
// T& value)` once per Method it Matches, in address order, exactly as
// AddressSpace::lookup does. Never allocates and throws nothing of its own.
template <typename T, typename Visitor>
LookupResult lookup(AddressSpace<T>& space, const OSCPP::Server::Message& message,
                    Visitor&& visitor)
{
    const ParseResult parsed = parsePattern(message);
    if (!parsed.ok()) {
        return LookupResult{0, parsed.error()};
    }
    return LookupResult{space.lookup(parsed.pattern(), std::forward<Visitor>(visitor)),
                        std::nullopt};
}

// As above, calling `visitor(std::string_view address, const T& value)`.
template <typename T, typename Visitor>
LookupResult lookup(const AddressSpace<T>& space, const OSCPP::Server::Message& message,
                    Visitor&& visitor)
{
    const ParseResult parsed = parsePattern(message);
    if (!parsed.ok()) {
        return LookupResult{0, parsed.error()};
    }
    return LookupResult{space.lookup(parsed.pattern(), std::forward<Visitor>(visitor)),
                        std::nullopt};
}

} // namespace oscpm
