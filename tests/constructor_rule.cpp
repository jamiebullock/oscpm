/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address.h>
#include <oscpm/pattern.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{

extern "C" void exitAfterAbort(int)
{
    std::_Exit(0);
}

}

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::signal(SIGABRT, exitAfterAbort);
    const std::string_view kind = argc > 2 ? argv[1] : "";
    const std::string_view text = argc > 2 ? argv[2] : "";
    if (kind == "address")
    {
        const oscpm::Address address(text);
        std::printf("constructed an address from %s\n", address.text().data());
    }
    else if (kind == "pattern")
    {
        const oscpm::Pattern pattern(text);
        std::printf("constructed a pattern from %s\n", pattern.text().data());
    }
    return 1;
}
