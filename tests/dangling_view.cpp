/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address.h>
#include <oscpm/pattern.h>

#include <string>

int main()
{
#if OSCPM_DANGLING_VIEW_CASE == 1
    const oscpm::Address address(std::string("/synth/1/freq"));
    return address.text().empty() ? 1 : 0;
#elif OSCPM_DANGLING_VIEW_CASE == 2
    const oscpm::Pattern pattern(std::string("/synth/*/freq"));
    return pattern.text().empty() ? 1 : 0;
#elif OSCPM_DANGLING_VIEW_CASE == 3
    const oscpm::Expected<oscpm::Address, oscpm::AddressError> address = oscpm::Address::parse(std::string("/synth/1/freq"));
    return address ? 0 : 1;
#elif OSCPM_DANGLING_VIEW_CASE == 4
    const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> pattern = oscpm::Pattern::parse(std::string("/synth/*/freq"));
    return pattern ? 0 : 1;
#endif
}
