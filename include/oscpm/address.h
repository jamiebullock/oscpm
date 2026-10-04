/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <oscpm/detail/validate.h>
#include <oscpm/error.h>

#include <optional>
#include <string_view>

namespace oscpm
{

/// A validated address. Holds a view of the caller's bytes, which must
/// outlive every use; copying the value copies the view.
class Address
{
public:
    class ParseResult;

    /// Parses `text`, yielding the address or the first fault.
    static constexpr ParseResult parse(std::string_view text) noexcept;

    /// The bytes this address was parsed from.
    constexpr std::string_view text() const noexcept
    {
        return m_text;
    }

private:
    constexpr Address() noexcept = default;

    constexpr explicit Address(std::string_view text) noexcept
        : m_text(text)
    {
    }

    std::string_view m_text;
};

/// An `Address` or the `Error` that stopped it parsing.
class Address::ParseResult
{
public:
    /// Whether parsing succeeded and `address()` holds the result.
    constexpr explicit operator bool() const noexcept
    {
        return m_parsed;
    }

    /// The address; meaningful only when the result is true.
    constexpr const Address& address() const noexcept
    {
        return m_address;
    }

    /// The fault; meaningful only when the result is false.
    constexpr Error error() const noexcept
    {
        return m_error;
    }

private:
    friend class Address;

    constexpr explicit ParseResult(Address address) noexcept
        : m_address(address)
        , m_parsed(true)
    {
    }

    constexpr explicit ParseResult(Error error) noexcept
        : m_error(error)
    {
    }

    Address m_address;
    Error m_error = Error::MissingLeadingSlash;
    bool m_parsed = false;
};

constexpr Address::ParseResult Address::parse(std::string_view text) noexcept
{
    const std::optional<Error> fault = detail::validateAddress(text);
    return fault.has_value() ? ParseResult(*fault) : ParseResult(Address(text));
}

}
