/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{

// The assertion has written its message by the time abort() raises SIGABRT,
// so exiting normally here leaves that message as the whole of the output.
extern "C" void exitAfterAssertion(int)
{
    std::_Exit(0);
}

}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
#ifdef NDEBUG
    std::puts("assertions are compiled out of this build, so the visitor rule is not checked");
    return 0;
#else
    const bool assign = argc > 1 && std::strcmp(argv[1], "assign") == 0;
    const bool moveFrom = argc > 1 && std::strcmp(argv[1], "move") == 0;
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::signal(SIGABRT, exitAfterAssertion);
    oscpm::AddressSpace<int> space;
    space.add("/a", 1);
    if (moveFrom)
    {
        space.dispatch("/a", [&](std::string_view, int&)
            {
                const oscpm::AddressSpace<int> taken(std::move(space));
                std::printf("the visitor moved from the space, which now holds %zu methods, and nothing asserted\n", taken.size()); });
        return 1;
    }
    if (assign)
    {
        oscpm::AddressSpace<int> other;
        space.dispatch("/a", [&](std::string_view, int&)
            { space = std::move(other); });
        std::puts("the visitor assigned to the space and nothing asserted");
        return 1;
    }
    space.dispatch("/a", [&](std::string_view address, int&)
        { space.remove(address); });
    std::puts("the visitor removed a method and nothing asserted");
    return 1;
#endif
}
