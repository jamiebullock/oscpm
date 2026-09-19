// Uses the adapter target: a Lookup straight from an oscpp Message. Both
// oscpm's and oscpp's headers must be on the include path for this to
// compile, which is what linking oscpm::oscpp promises.

#include <oscpm/oscpp.hpp>

#include <oscpp/client.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

int main()
{
    std::array<char, 64> buffer{};
    OSCPP::Client::Packet packet(buffer.data(), buffer.size());
    packet.openMessage("/synth/*/freq", 0).closeMessage();

    oscpm::AddressSpace<int> space;
    space.add("/synth/1/freq", 1);
    space.add("/synth/2/amp", 2);

    const OSCPP::Server::Message message(OSCPP::Server::Packet(packet.data(), packet.size()));
    const oscpm::LookupResult result =
        oscpm::lookup(space, message, [](std::string_view address, const int&) {
            std::printf("reached %.*s\n", static_cast<int>(address.size()), address.data());
        });

    return result.error || result.matched != 1;
}
