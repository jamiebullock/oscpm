// Dispatching oscpp messages through an oscpm Address Space, with no
// sockets: a bundle is built with oscpp's client API, parsed with its
// server API, and each message is looked up through the adapter. The
// program prints which Methods every message reaches and exits with 0.

#include <oscpm/oscpp.hpp>

#include <oscpp/client.hpp>

#include <cstddef>
#include <cstdio>
#include <string_view>

namespace {

// The value stored at each Method: a handler that receives the Address the
// message reached and the message's arguments.
using Handler = void (*)(std::string_view address, OSCPP::Server::ArgStream args);

void setFrequency(std::string_view address, OSCPP::Server::ArgStream args)
{
    std::printf("    %.*s <- frequency %g Hz\n", static_cast<int>(address.size()), address.data(),
                static_cast<double>(args.float32()));
}

void setAmplitude(std::string_view address, OSCPP::Server::ArgStream args)
{
    std::printf("    %.*s <- amplitude %g\n", static_cast<int>(address.size()), address.data(),
                static_cast<double>(args.float32()));
}

void setGain(std::string_view address, OSCPP::Server::ArgStream args)
{
    std::printf("    %.*s <- gain %g dB\n", static_cast<int>(address.size()), address.data(),
                static_cast<double>(args.float32()));
}

// Registration happens once, on the setup thread: `add` may allocate.
void registerMethods(oscpm::AddressSpace<Handler>& space)
{
    const char* const frequencies[] = {"/synth/1/freq", "/synth/2/freq", "/synth/10/freq"};
    const char* const amplitudes[] = {"/synth/1/amp", "/synth/2/amp", "/synth/10/amp"};
    for (const char* address : frequencies) {
        space.add(address, &setFrequency);
    }
    for (const char* address : amplitudes) {
        space.add(address, &setAmplitude);
    }
    space.add("/master/gain", &setGain);
    space.add("/synth/1/gain", &setGain);
}

// The packet a client might send: three well-formed patterns, one that
// reaches nothing, and one that is Malformed.
std::size_t buildPacket(void* buffer, std::size_t size)
{
    OSCPP::Client::Packet packet(buffer, size);
    packet.openBundle(1)
        .openMessage("/synth/?/freq", 1)
        .float32(440.0f)
        .closeMessage()
        .openMessage("/synth/{1,10}/amp", 1)
        .float32(0.5f)
        .closeMessage()
        .openMessage("//gain", 1)
        .float32(-6.0f)
        .closeMessage()
        .openMessage("/synth/3/freq", 1)
        .float32(220.0f)
        .closeMessage()
        .openMessage("/synth/[1/freq", 1)
        .float32(110.0f)
        .closeMessage()
        .closeBundle();
    return packet.size();
}

// The receiving side, as it would run per packet: `lookup` never allocates
// and never throws, so this could sit inside an audio callback.
void handleMessage(const oscpm::AddressSpace<Handler>& space,
                   const OSCPP::Server::Message& message)
{
    std::printf("%s\n", message.address());
    const oscpm::LookupResult result =
        oscpm::lookup(space, message, [&](std::string_view address, const Handler& handler) {
            handler(address, message.args());
        });
    if (result.error) {
        std::printf("    Malformed: %s at byte %zu\n", oscpm::toString(result.error->kind),
                    result.error->offset);
    } else if (result.matched == 0) {
        std::printf("    reached no Methods\n");
    }
}

void handlePacket(const oscpm::AddressSpace<Handler>& space, const OSCPP::Server::Packet& packet)
{
    if (packet.isBundle()) {
        const OSCPP::Server::Bundle bundle(packet);
        OSCPP::Server::PacketStream packets(bundle.packets());
        while (!packets.atEnd()) {
            handlePacket(space, packets.next());
        }
    } else {
        handleMessage(space, OSCPP::Server::Message(packet));
    }
}

} // namespace

int main()
{
    oscpm::AddressSpace<Handler> space;
    registerMethods(space);

    alignas(4) char buffer[512];
    const std::size_t size = buildPacket(buffer, sizeof(buffer));

    handlePacket(space, OSCPP::Server::Packet(buffer, size));
    return 0;
}
