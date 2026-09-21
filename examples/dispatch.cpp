/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>
#include <oscpm/oscpm.h>

#include <oscpp/client.hpp>
#include <oscpp/server.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

namespace
{

struct Parameter
{
    float value = 0.0f;
};

constexpr std::size_t kPacketBytes = 1024;
constexpr std::uint64_t kImmediately = 1;

std::size_t buildBundle(void* buffer, std::size_t size)
{
    OSCPP::Client::Packet packet(buffer, size);
    packet.openBundle(kImmediately)
        .openMessage("/synth/1/freq", 1)
        .float32(440.0f)
        .closeMessage()
        .openMessage("/synth/*/amp", 1)
        .float32(0.5f)
        .closeMessage()
        .openMessage("/mixer/{master,aux}/gain", 1)
        .float32(0.8f)
        .closeMessage()
        .openMessage("/synth/3/freq", 1)
        .float32(110.0f)
        .closeMessage()
        .openMessage("/synth/[1-2", 1)
        .float32(0.0f)
        .closeMessage()
        .closeBundle();
    return packet.size();
}

void dispatch(const OSCPP::Server::Message& message, oscpm::AddressSpace<Parameter>& parameters, const oscpm::Pattern& frequencyWatch)
{
    const std::string_view address = message.address();
    OSCPP::Server::ArgStream arguments(message.args());
    const float value = arguments.float32();
    const oscpm::DispatchResult result = parameters.dispatch(address, [&](std::string_view method, Parameter& parameter)
        {
            parameter.value = value;
            std::printf("%-26s sets %.*s to %g\n", message.address(), static_cast<int>(method.size()), method.data(), static_cast<double>(value)); });
    if (result.error)
    {
        std::printf("%-26s rejected: %s at byte %zu\n", message.address(), oscpm::toString(result.error->kind), result.error->offset);
        return;
    }
    if (result.matched == 0)
    {
        std::printf("%-26s matches no method\n", message.address());
    }
    if (frequencyWatch.matches(address))
    {
        std::printf("%-26s is a frequency change\n", message.address());
    }
}

void dispatchPacket(const OSCPP::Server::Packet& packet, oscpm::AddressSpace<Parameter>& parameters, const oscpm::Pattern& frequencyWatch)
{
    if (packet.isBundle())
    {
        OSCPP::Server::PacketStream packets(OSCPP::Server::Bundle(packet).packets());
        while (!packets.atEnd())
        {
            dispatchPacket(packets.next(), parameters, frequencyWatch);
        }
    }
    else
    {
        dispatch(OSCPP::Server::Message(packet), parameters, frequencyWatch);
    }
}

}

int main()
{
    oscpm::AddressSpace<Parameter> parameters;
    for (const char* address : { "/synth/1/freq", "/synth/1/amp", "/synth/2/freq", "/synth/2/amp", "/mixer/master/gain" })
    {
        if (const auto fault = parameters.add(address, Parameter { }))
        {
            std::printf("cannot register %s: %s\n", address, oscpm::toString(*fault));
            return 1;
        }
    }

    const oscpm::ParseResult frequencyWatch = oscpm::Pattern::parse("//freq");
    if (!frequencyWatch)
    {
        return 1;
    }

    std::array<char, kPacketBytes> buffer { };
    const std::size_t packetSize = buildBundle(buffer.data(), buffer.size());
    dispatchPacket(OSCPP::Server::Packet(buffer.data(), packetSize), parameters, frequencyWatch.pattern());

    std::printf("\n");
    parameters.forEach([](std::string_view address, const Parameter& parameter)
        { std::printf("%-26.*s = %g\n", static_cast<int>(address.size()), address.data(), static_cast<double>(parameter.value)); });
    return 0;
}
