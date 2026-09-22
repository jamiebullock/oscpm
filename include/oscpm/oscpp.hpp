// oscpm - OSC address pattern matching for C++
//
// Copyright (c) 2026 Jamie Bullock
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

// Glue between oscpm and oscpp (https://github.com/kaoskorobase/oscpp). Include this header only
// when <oscpp/server.hpp> is on your include path.

#pragma once

#include "address_space.hpp"
#include "pattern.hpp"

#include <oscpp/server.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace oscpm {

/// True if the address pattern carried by `message` matches the literal `address`.
inline bool match(const OSCPP::Server::Message& message, std::string_view address) noexcept
{
    return match(std::string_view(message.address()), address);
}

/// True if `message`'s address, taken as a literal, matches the receiver-side `pattern`. Useful
/// for filtering incoming messages by a subscription pattern.
inline bool match(const Pattern& pattern, const OSCPP::Server::Message& message) noexcept
{
    return pattern.matches(message.address());
}

/// Dispatches `message` against `space`, calling `f(const OSCPP::Server::Message&,
/// const std::string& address, T& value)` for every matching entry. Returns the number of calls.
template <class Space, class F>
std::size_t dispatch(Space& space, const OSCPP::Server::Message& message, F&& f)
{
    return space.dispatch(std::string_view(message.address()),
                          [&](const std::string& address, auto& value) { f(message, address, value); });
}

/// Dispatches every message in `packet` against `space`, recursing into bundles. Bundle time tags
/// are ignored; scheduling is left to the caller, as in oscpp. `f` is called as for the `Message`
/// overload. Returns the total number of calls.
template <class Space, class F>
std::size_t dispatch(Space& space, const OSCPP::Server::Packet& packet, F&& f)
{
    if (packet.isBundle()) {
        std::size_t count = 0;
        const OSCPP::Server::Bundle bundle(packet);
        OSCPP::Server::PacketStream packets(bundle.packets());
        while (!packets.atEnd())
            count += dispatch(space, packets.next(), f);
        return count;
    }
    return dispatch(space, OSCPP::Server::Message(packet), f);
}

} // namespace oscpm
