/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "address_space_model.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace k
{
constexpr std::uint32_t seed = 20261127;
constexpr int numRuns = 24;
constexpr std::size_t numBytesPerRun = 1024;
}

TEST_CASE("address spaces of every kind agree with a map over seeded random operations")
{
    std::mt19937 generator(k::seed);
    for (int run = 0; run < k::numRuns; ++run)
    {
        std::vector<std::uint8_t> bytes(k::numBytesPerRun);
        for (std::uint8_t& byte : bytes)
        {
            byte = static_cast<std::uint8_t>(generator());
        }
        oscpm_test::Choices choices(bytes.data(), bytes.size());
        INFO("run " << run);
        REQUIRE(oscpm_test::firstDifference(choices) == "");
    }
}
