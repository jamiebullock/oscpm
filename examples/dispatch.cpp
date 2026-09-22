// Builds an OSC bundle with oscpp and dispatches it through an oscpm::AddressSpace.

#include <oscpm/oscpp.hpp>

#include <oscpp/client.hpp>

#include <array>
#include <cstdio>
#include <functional>
#include <string>

using Handler = std::function<void(const std::string& address, OSCPP::Server::ArgStream args)>;

int main()
{
    oscpm::AddressSpace<Handler> space;
    space.add("/synth/1/freq", [](const std::string& a, OSCPP::Server::ArgStream args) {
        std::printf("%s <- %g\n", a.c_str(), args.float32());
    });
    space.add("/synth/2/freq", [](const std::string& a, OSCPP::Server::ArgStream args) {
        std::printf("%s <- %g\n", a.c_str(), args.float32());
    });
    space.add("/synth/2/amp", [](const std::string& a, OSCPP::Server::ArgStream args) {
        std::printf("%s <- %g\n", a.c_str(), args.float32());
    });
    space.add("/fx/reverb/mix", [](const std::string& a, OSCPP::Server::ArgStream args) {
        std::printf("%s <- %g\n", a.c_str(), args.float32());
    });

    std::array<char, 512> buffer{};
    OSCPP::Client::Packet out(buffer.data(), buffer.size());
    out.openBundle(0)
        .openMessage("/synth/*/freq", 1).float32(440.f).closeMessage()
        .openMessage("/synth/2/{amp,pan}", 1).float32(0.5f).closeMessage()
        .openMessage("//mix", 1).float32(0.25f).closeMessage()
    .closeBundle();

    const OSCPP::Server::Packet packet(buffer.data(), out.size());
    const std::size_t n = oscpm::dispatch(space, packet, [](const OSCPP::Server::Message& m, const std::string& address, Handler& handler) {
        handler(address, m.args());
    });
    std::printf("%zu handler(s) called\n", n);
    return 0;
}
