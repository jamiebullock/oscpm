/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/address_space.h>
#include <oscpm/pattern.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace
{

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

template <typename Error>
bool isNamed(Error error) noexcept
{
    return oscpm::toString(error)[0] != '\0';
}

void checkPattern(std::string_view pattern, std::string_view address)
{
    const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> parsed = oscpm::Pattern::parse(pattern);
    const bool convenience = oscpm::match(pattern, address);

    if (!parsed)
    {
        require(isNamed(parsed.error()));
        require(!convenience);
        return;
    }

    require(parsed->text() == pattern);
    const oscpm::Expected<oscpm::Address, oscpm::AddressError> parsedAddress = oscpm::Address::parse(address);
    if (!parsedAddress)
    {
        require(!convenience);
        return;
    }
    const bool byValue = parsed->matches(*parsedAddress);
    require(byValue == convenience);
    require(byValue == oscpm::detail::matchParsed(pattern, address));
}

void checkAddress(std::string_view address)
{
    const oscpm::Expected<oscpm::Address, oscpm::AddressError> parsed = oscpm::Address::parse(address);
    if (!parsed)
    {
        require(isNamed(parsed.error()));
        return;
    }
    require(parsed->text() == address);

    oscpm::AddressSpace<int, false> space;
    require(space.add(*parsed, 0));
    require(!space.add(*parsed, 0));
    require(space.size() == 1);
    require(space.find(address) != nullptr);

    const oscpm::Expected<oscpm::Pattern, oscpm::PatternError> self = oscpm::Pattern::parse(address);
    require(static_cast<bool>(self));
    require(self->isLiteral());
    require(self->matches(*parsed));
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const std::size_t newline = input.find('\n');
    const std::string_view pattern = input.substr(0, newline);
    const std::string_view address = newline == std::string_view::npos ? std::string_view() : input.substr(newline + 1);

    checkPattern(pattern, address);
    checkAddress(address);
    return 0;
}
