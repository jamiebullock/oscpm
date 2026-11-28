/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/oscpm.h>

#include <string_view>

// Declares `Serial` on a core that uses the Adafruit TinyUSB stack.
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif

namespace k
{
constexpr unsigned long baudRate = 115200;
constexpr unsigned long millisecondsBetweenMessages = 1000;
constexpr float frequency = 440.0f;
}

static_assert(oscpm::match("/synth/*/freq", "/synth/1/freq"), "a wildcard pattern matches at compile time");
static_assert(!oscpm::match("/synth/*/freq", "/synth/1/amp"), "a wildcard pattern rejects at compile time");

// A space without a memo: the default memo takes about 280 KB when the space is constructed.
oscpm::AddressSpace<float, false> parameters;

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
