/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address.h>
#include <oscpm/pattern.h>

#if OSCPM_CONSTRUCTOR_CASE == 1
constexpr oscpm::Address kAddress("/synth/1/");
#elif OSCPM_CONSTRUCTOR_CASE == 2
constexpr oscpm::Pattern kPattern("/synth/[1-3");
#endif

int main()
{
    return 0;
}
