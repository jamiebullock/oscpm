/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/validate.h>
#include <oscpm/error.h>
#include <oscpm/expected.h>

#include <optional>
#include <string_view>

namespace oscpm
{

/// A validated address. Holds a view of the caller's bytes, which must
/// outlive every use and stay unchanged, including through `AddressSpace::add`;
/// copying the value copies the view.
class Address
{
public:
    /// Parses `text`, yielding the address or the first fault.
    static constexpr Expected<Address, AddressError> parse(std::string_view text) noexcept;

    /// The bytes this address was parsed from.
    constexpr std::string_view text() const noexcept
    {
        return m_text;
    }

private:
    constexpr explicit Address(std::string_view text) noexcept
        : m_text(text)
    {
    }

    std::string_view m_text;
};

constexpr Expected<Address, AddressError> Address::parse(std::string_view text) noexcept
{
    const std::optional<AddressError> fault = detail::validateAddress(text);
    if (fault.has_value())
    {
        return *fault;
    }
    return Address(text);
}

}
