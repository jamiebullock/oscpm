/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>

namespace oscpm_test
{

/// The number of calls so far to the global allocation functions, which the
/// test binary replaces; a difference of zero across a call proves it did
/// not allocate.
std::size_t allocationCount() noexcept;

}
