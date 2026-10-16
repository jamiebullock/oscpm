/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define OSCPM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define OSCPM_NOINLINE __declspec(noinline)
#else
#define OSCPM_NOINLINE
#endif
