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

#define OSCPM_LIFETIMEBOUND
#if defined(__clang__)
#if __has_cpp_attribute(clang::lifetimebound)
#undef OSCPM_LIFETIMEBOUND
#define OSCPM_LIFETIMEBOUND [[clang::lifetimebound]]
#endif
#endif
