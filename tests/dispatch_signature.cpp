/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>

#include <utility>

namespace
{

struct MutableMethod
{
    void operator()(int) { }
};

}

int main()
{
    oscpm::AddressSpace<MutableMethod> space;
#if OSCPM_DISPATCH_CASE == 1
    space.dispatch("/a");
#elif OSCPM_DISPATCH_CASE == 2
    std::as_const(space).dispatch("/a", 1);
#endif
    return 0;
}
