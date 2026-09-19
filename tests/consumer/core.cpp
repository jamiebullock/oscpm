// Uses the core target only: the matcher, the Address Space and the
// version macros. Exits 0 when everything behaves.

#include <oscpm/address_space.hpp>
#include <oscpm/pattern.hpp>

#include <cstdio>
#include <cstring>
#include <string_view>

#if !defined(OSCPM_VERSION_MAJOR) || !defined(OSCPM_VERSION)
#error "oscpm/pattern.hpp should provide the version macros"
#endif

int main()
{
    if (oscpm::match("/synth/[1-4]/{freq,amp}", "/synth/2/freq") != oscpm::MatchResult::Match) {
        std::puts("match failed");
        return 1;
    }

    oscpm::AddressSpace<int> space;
    space.add("/synth/1/freq", 1);
    space.add("/synth/2/freq", 2);
    int sum = 0;
    space.lookup(oscpm::Pattern::parse("/synth/*/freq").pattern(),
                 [&](std::string_view, int& value) { sum += value; });
    if (sum != 3) {
        std::puts("lookup failed");
        return 1;
    }

    std::printf("oscpm %s\n", OSCPM_VERSION);
    return std::strlen(OSCPM_VERSION) == 0;
}
