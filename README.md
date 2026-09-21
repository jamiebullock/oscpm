# oscpm-regex

Header-only C++20 OpenSoundControl (OSC) address pattern matching built on
`std::regex`.

oscpm-regex translates each OSC address pattern, the `?`, `*`, `[...]` and
`{a,b}` syntax of OSC 1.0 plus the `//` operator of OSC 1.1, into an
anchored ECMAScript regular expression and matches it with the standard
library's engine. A registry adds a hash map for exact addresses and a cache
of pattern results, so that an address seen before costs one hash. It
depends on nothing outside the standard library.

## Integration

With CMake 3.25 or later, `add_subdirectory` on a checkout gives the target
`oscpm_regex::oscpm_regex`; `cmake --install` installs the header and a
package config for `find_package(oscpm_regex)`. The tests are built only when
oscpm-regex is the top-level project.

## Matching

```cpp
#include <oscpm_regex/oscpm_regex.h>

oscpm_regex::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm_regex::match("/synth//freq", "/synth/1/osc/freq"); // true

const oscpm_regex::Pattern pattern("/synth/[1-3]/{freq,amp}");
if (pattern.valid())
{
    pattern.matches("/synth/2/amp"); // true
}
else
{
    pattern.error(); // a std::optional<oscpm_regex::Error>
}
```

`match` compiles the pattern and tests it once. A `Pattern` compiles once
and is matched many times. A pattern that does not compile reports one of
`MissingLeadingSlash`, `IllegalByte`, `UnterminatedClass`,
`UnterminatedBraces`, `NestedBraces` or `RegexRejected`, and matches
nothing. `isValidAddress` applies the OSC address rules to an address.

## Registry

```cpp
oscpm_regex::Registry<Handler> methods;
methods.add("/synth/1/freq", setFrequency); // false if malformed or already registered
methods.remove("/synth/1/freq");            // false if not registered

const auto result = methods.dispatch(message.address(), [&](std::string_view address, Handler& handler)
    { handler(message); });
result.matched;   // how many methods were visited
result.malformed; // the pattern did not compile, and none were
```

`dispatch` looks a literal pattern up in a hash map of addresses, replays a
pattern it has seen since the last `add` or `remove` from a cache, and
otherwise compiles the pattern and matches it against every method. The
cache holds up to 4,096 patterns and is emptied when full.

## Guarantees and their limits

- The exact path and a cache hit allocate nothing, which the tests assert.
- A first sight of a wildcard pattern compiles a `std::regex` and calls
  `std::regex_match` once per method; both allocate, thousands of times
  against a thousand methods. There is no allocation-free cold path.
- Matching time is bounded by the regex engine; libc++'s does not
  backtrack catastrophically, so hostile patterns cost milliseconds, not
  seconds. The engine may throw `error_complexity` or `error_stack` on
  extreme inputs, which `matches` reports as no match.
- `Pattern` owns its compiled expression; the text it was built from need
  not outlive it.
- A `Registry` is not safe to use from several threads at once.

## Matching rules

A pattern and an address are split into parts on `/`. Within a part, `?`
matches any one byte, `*` any run of bytes, `[abc]` and `[a-z]` one byte
from the class with `!` negating, and `{foo,bar}` one of the listed
strings. None of them matches `/`. An empty part is the `//` operator and
matches zero or more whole parts: `/a//c` matches `/a/c` and `/a/b/c`;
consecutive empty parts collapse, and a trailing `/` is a trailing `//`, so
`/a/` matches `/a` and everything beneath it. Matching is by byte and
case-sensitive. A pattern must be printable ASCII.

## Building

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja. The tests use Catch2, fetched at
configure time. The build fails on a clang-format violation when
oscpm-regex is the top-level project.

## Licence

zlib. See `LICENSE`.
