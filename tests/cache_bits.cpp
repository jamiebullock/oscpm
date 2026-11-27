/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>

#include <cstddef>
#include <limits>

namespace
{

using OneBucketTooManyBits = oscpm::AddressSpace<int, true, std::numeric_limits<std::size_t>::digits>;

}

int main()
{
    return sizeof(OneBucketTooManyBits) == 0 ? 1 : 0;
}
