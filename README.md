# oscpm_flat

Header-only C++17 OpenSoundControl (OSC) address pattern matching: the `?`,
`*`, `[...]` and `{a,b}` syntax of OSC 1.0 and the OSC 1.1 `//` operator.

A pattern is compiled once into its parts: literal parts, brace lists
expanded to their alternatives, and a Glushkov automaton for each part with
a wildcard. A `Registry` stores each method's address with its part
boundaries, finds a literal pattern through a hash index, matches any other
pattern against every method, and memoises the result.

## Limits

A pattern is rejected when it exceeds any of:

| Limit | Value |
| --- | --- |
| parts | 32 (`kMaxParts`) |
| wildcard positions across the pattern | 128 (`kMaxPositions`) |
| combinations of the brace lists in one part | 16 (`kMaxAltProduct`) |
| bytes | 256 (`kMaxPatternBytes`) |

An address is split into at most 32 parts; the last part holds the rest.
A memo entry holds at most 32 results (`kInlineResults`); a larger result
is delivered in full but not memoised.

## Use

```cpp
#include <oscpm_flat/oscpm_flat.h>

bool hit = oscpm_flat::match("/synth/*/freq", "/synth/1/freq");

oscpm_flat::Registry<float> parameters;
parameters.add("/synth/1/freq", 440.0f);
const auto result = parameters.dispatch("/synth/*/freq", [](std::string_view address, float& value) {
    value = 220.0f;
});
// result.matched, result.malformed
```

## Build

```bash
cmake --preset release && cmake --build --preset release && ctest --preset release
```

The CMake target is `oscpm_flat::oscpm_flat`. `OSCPM_BUILD_TESTS` and
`OSCPM_CHECK_FORMAT` are on when this is the top-level project.

## Licence

Zlib; see `LICENSE`.
