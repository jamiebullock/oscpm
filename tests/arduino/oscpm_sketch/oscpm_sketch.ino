/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <cstddef>
#include <string_view>

namespace k
{
constexpr unsigned long baudRate = 115200;
constexpr unsigned long millisecondsBetweenMessages = 1000;
constexpr float frequency = 440.0f;
}

/// A view of a string literal with its length taken from the array type,
/// because libstdc++ 7 cannot measure a literal in a constant expression.
template <std::size_t N>
constexpr std::string_view literal(const char (&text)[N]) noexcept
{
    return std::string_view(text, N - 1);
}

static_assert(oscpm::match(literal("/synth/*/freq"), literal("/synth/1/freq")), "a wildcard pattern matches at compile time");
static_assert(!oscpm::match(literal("/synth/*/freq"), literal("/synth/1/amp")), "a wildcard pattern rejects at compile time");

oscpm::AddressSpace<float> parameters;

void setup()
{
    Serial.begin(k::baudRate);
    for (const char* text : { "/synth/1/freq", "/synth/2/freq", "/synth/1/amp" })
    {
        const oscpm::Expected<oscpm::Address, oscpm::AddressError> address = oscpm::Address::parse(text);
        if (address)
        {
            parameters.add(*address, 0.0f);
        }
    }
}

void loop()
{
    const oscpm::MatchResult result = parameters.visit("/synth/*/freq", [](std::string_view address, float& value)
        {
            value = k::frequency;
            Serial.write(address.data(), address.size());
            Serial.println(); });
    if (result.error)
    {
        Serial.println(oscpm::toString(*result.error));
    }
    Serial.println(result.matched);
    delay(k::millisecondsBetweenMessages);
}
