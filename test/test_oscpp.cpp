#include <oscpm/oscpp.hpp>

#include <oscpp/client.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

using oscpm::AddressSpace;

namespace {

    struct Received
    {
        std::string address;
        int value;
        int arg;
    };

} // namespace

TEST_CASE("oscpp Message matches against addresses and patterns")
{
    std::array<char, 128> buffer{};
    OSCPP::Client::Packet out(buffer.data(), buffer.size());
    out.openMessage("/synth/*/freq", 1).float32(440.f).closeMessage();

    const OSCPP::Server::Packet packet(buffer.data(), out.size());
    REQUIRE(packet.isMessage());
    const OSCPP::Server::Message message(packet);

    CHECK(oscpm::match(message, "/synth/1/freq"));
    CHECK_FALSE(oscpm::match(message, "/synth/1/amp"));

    const oscpm::Pattern subscription("/synth/*/*");
    CHECK(oscpm::match(subscription, message));
    CHECK_FALSE(oscpm::match(oscpm::Pattern("/fx/*"), message));
}

TEST_CASE("dispatching a single oscpp message")
{
    AddressSpace<int> space;
    space.add("/synth/1/freq", 1);
    space.add("/synth/2/freq", 2);
    space.add("/synth/2/amp", 3);

    std::array<char, 128> buffer{};
    OSCPP::Client::Packet out(buffer.data(), buffer.size());
    out.openMessage("/synth/?/freq", 1).int32(7).closeMessage();

    const OSCPP::Server::Packet packet(buffer.data(), out.size());
    std::vector<Received> received;
    const std::size_t n = oscpm::dispatch(space, packet, [&](const OSCPP::Server::Message& m, const std::string& address, int& value) {
        received.push_back({address, value, m.args().int32()});
    });

    CHECK(n == 2);
    REQUIRE(received.size() == 2);
    CHECK(received[0].address == "/synth/1/freq");
    CHECK(received[0].value == 1);
    CHECK(received[0].arg == 7);
    CHECK(received[1].address == "/synth/2/freq");
    CHECK(received[1].value == 2);
    CHECK(received[1].arg == 7);
}

TEST_CASE("dispatching a nested oscpp bundle")
{
    AddressSpace<int> space;
    space.add("/synth/1/freq", 1);
    space.add("/synth/2/freq", 2);
    space.add("/synth/2/amp", 3);
    space.add("/fx/mix", 4);

    std::array<char, 512> buffer{};
    OSCPP::Client::Packet out(buffer.data(), buffer.size());
    out.openBundle(1)
        .openMessage("/synth/*/freq", 1).int32(1).closeMessage()
        .openBundle(2)
            .openMessage("/fx/mix", 1).int32(2).closeMessage()
            .openMessage("/nothing/here", 1).int32(3).closeMessage()
        .closeBundle()
        .openMessage("//amp", 1).int32(4).closeMessage()
    .closeBundle();

    const OSCPP::Server::Packet packet(buffer.data(), out.size());
    REQUIRE(packet.isBundle());

    std::vector<Received> received;
    const std::size_t n = oscpm::dispatch(space, packet, [&](const OSCPP::Server::Message& m, const std::string& address, const int& value) {
        received.push_back({address, value, m.args().int32()});
    });

    CHECK(n == 4);
    REQUIRE(received.size() == 4);
    CHECK(received[0].address == "/synth/1/freq");
    CHECK(received[0].arg == 1);
    CHECK(received[1].address == "/synth/2/freq");
    CHECK(received[1].arg == 1);
    CHECK(received[2].address == "/fx/mix");
    CHECK(received[2].value == 4);
    CHECK(received[2].arg == 2);
    CHECK(received[3].address == "/synth/2/amp");
    CHECK(received[3].arg == 4);
}

TEST_CASE("dispatching against a const space")
{
    AddressSpace<int> mutable_space;
    mutable_space.add("/a", 1);
    const AddressSpace<int>& space = mutable_space;

    std::array<char, 64> buffer{};
    OSCPP::Client::Packet out(buffer.data(), buffer.size());
    out.openMessage("/a", 0).closeMessage();
    const OSCPP::Server::Packet packet(buffer.data(), out.size());

    int seen = 0;
    CHECK(oscpm::dispatch(space, packet, [&](const OSCPP::Server::Message&, const std::string&, const int& v) { seen = v; }) == 1);
    CHECK(seen == 1);
}
