/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/expected.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{

enum class Fault
{
    Missing
};

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
    const std::string_view side = argc > 1 ? argv[1] : "";
    if (side == "value")
    {
        const oscpm::Expected<int, Fault> fault = Fault::Missing;
        std::printf("read %d from an expected holding an error\n", *fault);
    }
    else if (side == "error")
    {
        const oscpm::Expected<int, Fault> value = 1;
        std::printf("read %d from an expected holding a value\n", static_cast<int>(value.error()));
    }
    return 1;
}
