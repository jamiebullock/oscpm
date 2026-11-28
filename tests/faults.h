/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/expected.h>
#include <oscpm/pattern.h>

#include <optional>

namespace oscpm_test
{

template <typename Value, typename Error>
constexpr bool faults(const oscpm::Expected<Value, Error>& result, Error kind)
{
    return !result && result.error() == kind;
}

constexpr bool faults(const std::optional<oscpm::PatternError>& result, oscpm::PatternError kind)
{
    return result == kind;
}

}
