/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address.h>
#include <oscpm/address_space.h>
#include <oscpm/pattern.h>

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

void handle(const OSCPP::Server::Message& message, oscpm::AddressSpace<Parameter>& parameters, const oscpm::Pattern& frequencyWatch)
{
    const std::string_view address = message.address();
    OSCPP::Server::ArgStream arguments(message.args());
    const float value = arguments.float32();
    const oscpm::MatchResult result = parameters.visit(address, [&](std::string_view registered, Parameter& parameter)
        {
            parameter.value = value;
            std::printf("%-26s sets %.*s to %g\n", message.address(), static_cast<int>(registered.size()), registered.data(), static_cast<double>(value)); });
    if (result.error)
    {
        std::printf("%-26s rejected: %s\n", message.address(), oscpm::toString(*result.error));
        return;
    }
    if (result.matched == 0)
    {
        std::printf("%-26s matches no address\n", message.address());
    }
    if (frequencyWatch.matches(address))
    {
        std::printf("%-26s is a frequency change\n", message.address());
    }
}

void handlePacket(const OSCPP::Server::Packet& packet, oscpm::AddressSpace<Parameter>& parameters, const oscpm::Pattern& frequencyWatch)
{
    if (packet.isBundle())
    {
        OSCPP::Server::PacketStream packets(OSCPP::Server::Bundle(packet).packets());
        while (!packets.atEnd())
        {
            handlePacket(packets.next(), parameters, frequencyWatch);
        }
    }
    else
    {
        handle(OSCPP::Server::Message(packet), parameters, frequencyWatch);
    }
}

}

int main()
{
    oscpm::AddressSpace<Parameter> parameters;
    for (const char* text : { "/synth/1/freq", "/synth/1/amp", "/synth/2/freq", "/synth/2/amp", "/mixer/master/gain" })
    {
        const oscpm::Expected<oscpm::Address, oscpm::AddressError> address = oscpm::Address::parse(text);
        if (!address)
        {
            std::printf("%s is not an address: %s\n", text, oscpm::toString(address.error()));
            return 1;
        }
        parameters.add(*address, Parameter { });
    }

    const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> frequencyWatch = oscpm::Pattern::parse("//freq");
    if (!frequencyWatch)
    {
        return 1;
    }

    std::array<char, kPacketBytes> buffer { };
    const std::size_t packetSize = buildBundle(buffer.data(), buffer.size());
    handlePacket(OSCPP::Server::Packet(buffer.data(), packetSize), parameters, *frequencyWatch);

    std::printf("\n");
    parameters.visit([](std::string_view address, const Parameter& parameter)
        { std::printf("%-26.*s = %g\n", static_cast<int>(address.size()), address.data(), static_cast<double>(parameter.value)); });
    return 0;
}
