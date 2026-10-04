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

bool isNamed(oscpm::Error error) noexcept
{
    return oscpm::toString(error)[0] != '\0';
}

void checkPattern(std::string_view pattern, std::string_view address)
{
    const oscpm::Pattern::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const bool convenience = oscpm::match(pattern, address);

    if (!parsed)
    {
        require(isNamed(parsed.error()));
        require(!convenience);
        return;
    }

    require(parsed.pattern().text() == pattern);
    const bool byValue = parsed.pattern().matches(address);
    require(byValue == convenience);
    require(byValue == oscpm::detail::matchParsed(pattern, address));
}

void checkAddress(std::string_view address)
{
    const oscpm::Address::ParseResult parsed = oscpm::Address::parse(address);
    if (!parsed)
    {
        require(isNamed(parsed.error()));
        return;
    }
    require(parsed.address().text() == address);

    oscpm::AddressSpace<int, false> space;
    require(space.add(parsed.address(), 0));
    require(!space.add(parsed.address(), 0));
    require(space.size() == 1);
    require(space.find(address) != nullptr);

    const oscpm::Pattern::ParseResult self = oscpm::Pattern::parse(address);
    require(static_cast<bool>(self));
    require(self.pattern().isLiteral());
    require(self.pattern().matches(address));
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
