/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "address_space_model.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    oscpm_test::Choices choices(data, size);
    const std::string difference = oscpm_test::firstDifference(choices);
    if (!difference.empty())
    {
        std::fprintf(stderr, "%s\n", difference.c_str());
        std::abort();
    }
    return 0;
}
