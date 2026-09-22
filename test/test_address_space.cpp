#include <oscpm/address_space.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <vector>

using oscpm::AddressSpace;
using oscpm::ErrorCode;
using oscpm::Pattern;

namespace {

    template <class Space>
    std::vector<std::string> addresses_matching(Space& space, std::string_view pattern)
    {
        std::vector<std::string> out;
        space.dispatch(pattern, [&](const std::string& address, auto&) { out.push_back(address); });
        return out;
    }

    AddressSpace<int> make_space()
    {
        AddressSpace<int> space;
        space.add("/synth/1/freq", 1);
        space.add("/synth/1/amp", 2);
        space.add("/synth/2/freq", 3);
        space.add("/synth/2/amp", 4);
        space.add("/synth/master/gain", 5);
        space.add("/fx/reverb/mix", 6);
        return space;
    }

} // namespace

TEST_CASE("add validates addresses")
{
    AddressSpace<int> space;
    CHECK_FALSE(space.add("/a", 1));
    CHECK(space.add("", 1).code == ErrorCode::empty);
    CHECK(space.add("a", 1).code == ErrorCode::missing_leading_slash);
    CHECK(space.add("/a/*", 1).code == ErrorCode::reserved_character);
    CHECK(space.add("/a/", 1).code == ErrorCode::empty_part);
    CHECK(space.size() == 1);
}

TEST_CASE("add replaces an existing entry")
{
    AddressSpace<int> space;
    space.add("/a", 1);
    space.add("/a", 2);
    CHECK(space.size() == 1);
    REQUIRE(space.find("/a") != nullptr);
    CHECK(*space.find("/a") == 2);
}

TEST_CASE("find, contains, remove")
{
    AddressSpace<int> space = make_space();
    CHECK(space.contains("/synth/1/freq"));
    CHECK_FALSE(space.contains("/synth/1"));
    CHECK(space.find("/nope") == nullptr);
    REQUIRE(space.find("/fx/reverb/mix") != nullptr);
    CHECK(*space.find("/fx/reverb/mix") == 6);

    const AddressSpace<int>& cspace = space;
    REQUIRE(cspace.find("/synth/2/amp") != nullptr);
    CHECK(*cspace.find("/synth/2/amp") == 4);

    CHECK(space.remove("/synth/1/freq"));
    CHECK_FALSE(space.remove("/synth/1/freq"));
    CHECK_FALSE(space.contains("/synth/1/freq"));
    CHECK(space.size() == 5);

    space.clear();
    CHECK(space.empty());
}

TEST_CASE("dispatch with a literal pattern hits exactly one entry")
{
    AddressSpace<int> space = make_space();
    CHECK(addresses_matching(space, "/synth/1/freq") == std::vector<std::string>{"/synth/1/freq"});
    CHECK(addresses_matching(space, "/synth/1").empty());
    CHECK(addresses_matching(space, "/synth/1/fre").empty());
}

TEST_CASE("dispatch visits wildcard matches in address order")
{
    AddressSpace<int> space = make_space();
    CHECK(addresses_matching(space, "/synth/*/freq") == std::vector<std::string>{"/synth/1/freq", "/synth/2/freq"});
    CHECK(addresses_matching(space, "/synth/[12]/*") ==
          std::vector<std::string>{"/synth/1/amp", "/synth/1/freq", "/synth/2/amp", "/synth/2/freq"});
    CHECK(addresses_matching(space, "//amp") == std::vector<std::string>{"/synth/1/amp", "/synth/2/amp"});
    CHECK(addresses_matching(space, "/*/*/*").size() == 6);
    CHECK(addresses_matching(space, "//*").size() == 6);
    CHECK(addresses_matching(space, "/{fx,synth}/*/mix") == std::vector<std::string>{"/fx/reverb/mix"});
    CHECK(addresses_matching(space, "/synth/*").empty());
}

TEST_CASE("dispatch returns the number of matches and passes mutable values")
{
    AddressSpace<int> space = make_space();
    const std::size_t n = space.dispatch("/synth/*/amp", [](const std::string&, int& value) { value *= 10; });
    CHECK(n == 2);
    CHECK(*space.find("/synth/1/amp") == 20);
    CHECK(*space.find("/synth/2/amp") == 40);
    CHECK(*space.find("/synth/1/freq") == 1);
}

TEST_CASE("dispatch on a const space passes const values")
{
    const AddressSpace<int> space = make_space();
    int sum = 0;
    const std::size_t n = space.dispatch("/synth/[12]/*", [&](const std::string&, const int& value) { sum += value; });
    CHECK(n == 4);
    CHECK(sum == 10);
}

TEST_CASE("dispatch accepts a compiled Pattern")
{
    AddressSpace<int> space = make_space();
    const Pattern literal("/fx/reverb/mix");
    const Pattern wild("/synth/?/freq");
    std::vector<std::string> out;
    CHECK(space.dispatch(literal, [&](const std::string& a, int&) { out.push_back(a); }) == 1);
    CHECK(space.dispatch(wild, [&](const std::string& a, int&) { out.push_back(a); }) == 2);
    CHECK(out == std::vector<std::string>{"/fx/reverb/mix", "/synth/1/freq", "/synth/2/freq"});
}

TEST_CASE("prefix narrowing does not skip matches")
{
    AddressSpace<int> space;
    space.add("/a", 0);
    space.add("/a/b", 0);
    space.add("/a/b/c", 0);
    space.add("/ab", 0);
    space.add("/b/a", 0);
    CHECK(addresses_matching(space, "/a/*") == std::vector<std::string>{"/a/b"});
    CHECK(addresses_matching(space, "/a*") == std::vector<std::string>{"/a", "/ab"});
    CHECK(addresses_matching(space, "/a//c") == std::vector<std::string>{"/a/b/c"});
    CHECK(addresses_matching(space, "//a") == std::vector<std::string>{"/a", "/b/a"});
    CHECK(addresses_matching(space, "/[ab]") == std::vector<std::string>{"/a"});
}

TEST_CASE("values can be callables")
{
    AddressSpace<std::function<void(float)>> space;
    float freq = 0.f;
    float amp = 0.f;
    space.add("/synth/freq", [&](float v) { freq = v; });
    space.add("/synth/amp", [&](float v) { amp = v; });
    space.dispatch("/synth/*", [](const std::string&, auto& handler) { handler(0.5f); });
    CHECK(freq == 0.5f);
    CHECK(amp == 0.5f);
}

TEST_CASE("iteration is in address order")
{
    AddressSpace<int> space = make_space();
    std::vector<std::string> keys;
    for (const auto& [address, value] : space)
        keys.push_back(address);
    CHECK(keys == std::vector<std::string>{"/fx/reverb/mix", "/synth/1/amp", "/synth/1/freq", "/synth/2/amp",
                                           "/synth/2/freq", "/synth/master/gain"});
}
