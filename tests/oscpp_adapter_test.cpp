// The oscpp adapter: a Pattern or a Lookup straight from an oscpp server
// Message. The adapter has no matching logic of its own, so this is a smoke
// test over packets built with oscpp's client API and parsed with its
// server API.

#include <oscpm/oscpp.hpp>

#include "allocation_counter.hpp"

#include <oscpp/client.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// A single-message packet built with oscpp's client API. The buffer is
// owned here because a server Message, and a Pattern parsed from it, view
// the packet's bytes.
class MessagePacket {
public:
    explicit MessagePacket(const char* address)
    {
        OSCPP::Client::Packet packet(m_buffer, sizeof(m_buffer));
        packet.openMessage(address, 1).float32(0.5f).closeMessage();
        m_size = packet.size();
    }

    OSCPP::Server::Message message() const
    {
        return OSCPP::Server::Message(OSCPP::Server::Packet(m_buffer, m_size));
    }

private:
    alignas(4) char m_buffer[256];
    std::size_t m_size = 0;
};

oscpm::AddressSpace<int> synthSpace()
{
    oscpm::AddressSpace<int> space;
    REQUIRE_FALSE(space.add("/synth/1/freq", 1).has_value());
    REQUIRE_FALSE(space.add("/synth/1/amp", 2).has_value());
    REQUIRE_FALSE(space.add("/synth/2/freq", 3).has_value());
    REQUIRE_FALSE(space.add("/master/gain", 4).has_value());
    return space;
}

} // namespace

TEST_CASE("parsePattern yields a Pattern over the message's address", "[oscpp]")
{
    const MessagePacket packet("/synth/*/freq");
    const OSCPP::Server::Message message = packet.message();

    const oscpm::ParseResult parsed = oscpm::parsePattern(message);
    REQUIRE(parsed.ok());
    CHECK(parsed.pattern().text() == "/synth/*/freq");
    CHECK(parsed.pattern().text().data() == message.address());
    CHECK(parsed.pattern().matches("/synth/1/freq") == oscpm::MatchResult::Match);
    CHECK(parsed.pattern().matches("/synth/1/amp") == oscpm::MatchResult::NoMatch);
}

TEST_CASE("parsePattern reports a Malformed address pattern with its offset", "[oscpp]")
{
    const MessagePacket packet("/synth/[1/freq");
    const OSCPP::Server::Message message = packet.message();

    const oscpm::ParseResult parsed = oscpm::parsePattern(message);
    REQUIRE_FALSE(parsed.ok());
    CHECK(parsed.error().kind == oscpm::ErrorKind::UnterminatedCharacterClass);
    CHECK(parsed.error().offset == 7);
}

TEST_CASE("lookup for a message reaches every Method the pattern Matches", "[oscpp]")
{
    oscpm::AddressSpace<int> space = synthSpace();
    const MessagePacket packet("/synth/?/freq");

    std::vector<std::pair<std::string, int>> visited;
    const oscpm::LookupResult result =
        oscpm::lookup(space, packet.message(), [&](std::string_view address, int& value) {
            visited.emplace_back(std::string(address), value);
        });

    CHECK_FALSE(result.error.has_value());
    CHECK(result.matched == 2);
    REQUIRE(visited.size() == 2);
    CHECK(visited[0] == std::pair<std::string, int>{"/synth/1/freq", 1});
    CHECK(visited[1] == std::pair<std::string, int>{"/synth/2/freq", 3});
}

TEST_CASE("lookup for a message has a const overload", "[oscpp]")
{
    const oscpm::AddressSpace<int> space = synthSpace();
    const MessagePacket packet("//gain");

    int seen = 0;
    const oscpm::LookupResult result =
        oscpm::lookup(space, packet.message(), [&](std::string_view address, const int& value) {
            CHECK(address == "/master/gain");
            seen = value;
        });

    CHECK_FALSE(result.error.has_value());
    CHECK(result.matched == 1);
    CHECK(seen == 4);
}

TEST_CASE("lookup for a message that reaches no Method visits nothing and reports no error", "[oscpp]")
{
    oscpm::AddressSpace<int> space = synthSpace();
    const MessagePacket packet("/synth/3/freq");

    const oscpm::LookupResult result = oscpm::lookup(
        space, packet.message(), [](std::string_view, int&) { FAIL("visitor called"); });

    CHECK_FALSE(result.error.has_value());
    CHECK(result.matched == 0);
}

TEST_CASE("lookup for a Malformed message reports the Error and visits nothing", "[oscpp]")
{
    oscpm::AddressSpace<int> space = synthSpace();
    const MessagePacket packet("/synth/1/freq/");

    const oscpm::LookupResult result = oscpm::lookup(
        space, packet.message(), [](std::string_view, int&) { FAIL("visitor called"); });

    REQUIRE(result.error.has_value());
    CHECK(result.error->kind == oscpm::ErrorKind::TrailingSlash);
    CHECK(result.error->offset == 13);
    CHECK(result.matched == 0);
}

TEST_CASE("the adapter does not allocate", "[oscpp][realtime]")
{
    oscpm::AddressSpace<int> space = synthSpace();
    const MessagePacket wildcards("/synth/*/{freq,amp}");
    const MessagePacket malformed("/synth/{a");
    const OSCPP::Server::Message wildcardMessage = wildcards.message();
    const OSCPP::Server::Message malformedMessage = malformed.message();
    int sum = 0;

    const std::size_t before = oscpm_test::allocationCount();

    const oscpm::ParseResult parsed = oscpm::parsePattern(wildcardMessage);
    const oscpm::ParseResult rejected = oscpm::parsePattern(malformedMessage);
    const oscpm::LookupResult found =
        oscpm::lookup(space, wildcardMessage, [&](std::string_view, int& value) { sum += value; });
    const oscpm::LookupResult refused =
        oscpm::lookup(space, malformedMessage, [&](std::string_view, int&) { sum = -1; });

    CHECK(oscpm_test::allocationCount() == before);
    CHECK(parsed.ok());
    CHECK_FALSE(rejected.ok());
    CHECK(found.matched == 3);
    CHECK(sum == 6);
    CHECK(refused.error.has_value());
}

TEST_CASE("a bundle's messages each reach their Methods through the adapter", "[oscpp]")
{
    oscpm::AddressSpace<int> space = synthSpace();

    alignas(4) char buffer[512];
    OSCPP::Client::Packet client(buffer, sizeof(buffer));
    client.openBundle(1)
        .openMessage("/synth/1/freq", 1)
        .float32(440.0f)
        .closeMessage()
        .openMessage("/synth/*/amp", 1)
        .float32(0.5f)
        .closeMessage()
        .openMessage("//gain", 0)
        .closeMessage()
        .closeBundle();

    const OSCPP::Server::Packet packet(buffer, client.size());
    REQUIRE(packet.isBundle());
    const OSCPP::Server::Bundle bundle(packet);
    OSCPP::Server::PacketStream packets(bundle.packets());

    std::vector<int> reached;
    while (!packets.atEnd()) {
        const OSCPP::Server::Message message(packets.next());
        const oscpm::LookupResult result = oscpm::lookup(
            space, message, [&](std::string_view, int& value) { reached.push_back(value); });
        CHECK_FALSE(result.error.has_value());
    }
    CHECK(reached == std::vector<int>{1, 2, 4});
}
