/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

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

bool pointsInto(const oscpm::ParseError& error, std::string_view text) noexcept
{
    return error.offset < text.size() || (text.empty() && error.offset == 0);
}

bool isNamed(oscpm::Error error) noexcept
{
    return oscpm::toString(error)[0] != '\0';
}

void checkPattern(std::string_view pattern, std::string_view address)
{
    const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const std::optional<oscpm::ParseError> fault = oscpm::validatePattern(pattern);
    const bool convenience = oscpm::match(pattern, address);

    if (!parsed)
    {
        require(fault.has_value());
        require(fault->kind == parsed.error().kind);
        require(fault->offset == parsed.error().offset);
        require(pointsInto(parsed.error(), pattern));
        require(isNamed(parsed.error().kind));
        require(!convenience);
        return;
    }

    require(!fault.has_value());
    require(parsed.pattern().text() == pattern);
    const bool byValue = parsed.pattern().matches(address);
    require(byValue == convenience);
    require(byValue == oscpm::detail::matchParsed(pattern, address));
}

void checkAddress(std::string_view address)
{
    const std::optional<oscpm::ParseError> fault = oscpm::validateAddress(address);
    if (fault)
    {
        require(pointsInto(*fault, address));
        require(isNamed(fault->kind));
        return;
    }

    const oscpm::ParseResult self = oscpm::Pattern::parse(address);
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
