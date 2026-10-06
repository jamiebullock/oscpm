/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>

#include <string_view>
#include <utility>

int main()
{
    oscpm::AddressSpace<int> space;
    [[maybe_unused]] const auto withoutAddress = [](int&) { };
    [[maybe_unused]] const auto mutating = [](std::string_view, int&) { };
#if OSCPM_VISITOR_CASE == 1
    space.dispatch("/a", withoutAddress);
#elif OSCPM_VISITOR_CASE == 2
    std::as_const(space).dispatch("/a", mutating);
#elif OSCPM_VISITOR_CASE == 3
    space.forEach(withoutAddress);
#elif OSCPM_VISITOR_CASE == 4
    std::as_const(space).forEach(mutating);
#endif
    return 0;
}
