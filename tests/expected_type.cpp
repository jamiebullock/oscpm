/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/expected.h>

#include <cstddef>

namespace
{

enum Unscoped
{
    UnscopedFault
};

enum class Fault
{
    Missing
};

struct FromFault
{
    FromFault(Fault)
    {
    }
};

}

int main()
{
#if OSCPM_EXPECTED_TYPE_CASE == 1
    [[maybe_unused]] constexpr std::size_t size = sizeof(oscpm::Expected<int, int>);
#elif OSCPM_EXPECTED_TYPE_CASE == 2
    [[maybe_unused]] constexpr std::size_t size = sizeof(oscpm::Expected<int, Unscoped>);
#elif OSCPM_EXPECTED_TYPE_CASE == 3
    [[maybe_unused]] constexpr std::size_t size = sizeof(oscpm::Expected<FromFault, Fault>);
#endif
    return 0;
}
