/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#undef NDEBUG

#include <oscpm/address.h>
#include <oscpm/address_space.h>
#include <oscpm/expected.h>
#include <oscpm/pattern.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
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

// A rule passes when the program aborts. An assertion has written its message
// by the time abort() raises SIGABRT, so exiting normally here leaves that
// message as the whole of the output.
extern "C" void exitAfterAbort(int)
{
    std::_Exit(0);
}

int removeFromVisitor()
{
    const oscpm::Address address = *oscpm::Address::parse("/a");
    oscpm::AddressSpace<int> space;
    space.add(address, 1);
    space.visit("/a", [&](std::string_view, int&)
        { space.remove(address); });
    std::puts("the visitor removed an address and nothing asserted");
    return 1;
}

int changeParsedAddress()
{
    std::string text = "/a";
    const oscpm::Address address = *oscpm::Address::parse(text);
    text.back() = '/';
    oscpm::AddressSpace<int> space;
    space.add(address, 1);
    std::puts("the address's bytes were changed and nothing asserted");
    return 1;
}

int readValueOfError()
{
    const oscpm::Expected<int, Fault> fault = Fault::Missing;
    std::printf("read %d from an expected holding an error\n", *fault);
    return 1;
}

int readErrorOfValue()
{
    const oscpm::Expected<int, Fault> value = 1;
    std::printf("read %d from an expected holding a value\n", static_cast<int>(value.error()));
    return 1;
}

int constructAddress(std::string_view text)
{
    const oscpm::Address address(text);
    std::printf("constructed an address from %s\n", address.text().data());
    return 1;
}

int constructPattern(std::string_view text)
{
    const oscpm::Pattern pattern(text);
    std::printf("constructed a pattern from %s\n", pattern.text().data());
    return 1;
}

int runRule(std::string_view rule, int numTexts, char** texts)
{
    if (rule == "remove-from-visitor" && numTexts == 0)
    {
        return removeFromVisitor();
    }
    if (rule == "change-parsed-address" && numTexts == 0)
    {
        return changeParsedAddress();
    }
    if (rule == "read-value-of-error" && numTexts == 0)
    {
        return readValueOfError();
    }
    if (rule == "read-error-of-value" && numTexts == 0)
    {
        return readErrorOfValue();
    }
    if (rule == "construct-address" && numTexts == 1)
    {
        return constructAddress(texts[0]);
    }
    if (rule == "construct-pattern" && numTexts == 1)
    {
        return constructPattern(texts[0]);
    }
    std::fprintf(stderr, "no rule \"%.*s\" takes %d text arguments\n", static_cast<int>(rule.size()), rule.data(), numTexts);
    return 1;
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
    if (argc < 2)
    {
        std::fputs("usage: oscpm_rules <rule> [text]\n", stderr);
        return 1;
    }
    return runRule(argv[1], argc - 2, argv + 2);
}
