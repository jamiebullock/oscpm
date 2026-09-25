/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/pattern.h>

int main()
{
    return oscpm::match("/synth/*/freq", "/synth/1/freq") ? 0 : 1;
}
