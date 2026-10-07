/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>

#include <mutex>

namespace
{

struct ConstMember
{
    const int value;
};

[[maybe_unused]] auto makeHandler()
{
    return [](int) { };
}

}

int main()
{
#if OSCPM_VALUE_TYPE_CASE == 1
    [[maybe_unused]] oscpm::AddressSpace<ConstMember> space;
#elif OSCPM_VALUE_TYPE_CASE == 2
    [[maybe_unused]] oscpm::AddressSpace<decltype(makeHandler())> space;
#elif OSCPM_VALUE_TYPE_CASE == 3
    [[maybe_unused]] oscpm::AddressSpace<std::mutex> space;
#elif OSCPM_VALUE_TYPE_CASE == 4
    [[maybe_unused]] oscpm::AddressSpace<int&> space;
#endif
    return 0;
}
