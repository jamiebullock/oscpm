/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <oscpm/expected.h>

#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <utility>

using oscpm::Expected;

namespace
{

enum class Fault
{
    Missing,
    Malformed
};

struct Point
{
    int x;
    int y;
};

constexpr Expected<Point, Fault> kPoint = Point { 3, 4 };
constexpr Expected<Point, Fault> kFault = Fault::Missing;

static_assert(kPoint.has_value());
static_assert(static_cast<bool>(kPoint));
static_assert((*kPoint).x == 3);
static_assert(kPoint->y == 4);
static_assert(!kFault.has_value());
static_assert(!static_cast<bool>(kFault));
static_assert(kFault.error() == Fault::Missing);
static_assert(std::is_trivially_copyable_v<Expected<Point, Fault>>);

}

TEST_CASE("an expected holding a value gives access to it")
{
    Expected<std::string, Fault> text = std::string("/synth/freq");
    REQUIRE(text);
    CHECK(*text == "/synth/freq");
    CHECK(text->size() == 11);

    text->append("/1");
    (*text).append("/2");
    CHECK(*text == "/synth/freq/1/2");
}

TEST_CASE("an expected holding an error gives the error")
{
    const Expected<std::string, Fault> text = Fault::Malformed;
    REQUIRE_FALSE(text);
    CHECK(text.error() == Fault::Malformed);
}

TEST_CASE("an expected's value can be moved out")
{
    const std::string longText(64, 'a');
    Expected<std::string, Fault> text = longText;
    const char* const buffer = text->data();
    const std::string moved = *std::move(text);
    CHECK(moved == longText);
    CHECK(moved.data() == buffer);
}
