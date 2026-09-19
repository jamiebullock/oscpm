# oscpm

Header-only C++17 OpenSoundControl (OSC) address pattern matching.

oscpm matches OSC address patterns against OSC addresses: the `?`, `*`,
`[...]` and `{a,b}` syntax of OSC 1.0, plus the `//` operator that OSC 1.1
took from XPath. It complements [oscpp](https://github.com/kaoskorobase/oscpp),
which reads and writes OSC packets but leaves address matching to the caller.
oscpm is usable on its own and depends on nothing outside the standard library.

## Usage

```cpp
#include <oscpm/oscpm.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true
oscpm::match("/synth/[1-3]/{freq,amp}", "/synth/2/amp"); // true
```

`match(pattern, address)` takes two `std::string_view`s, allocates nothing,
never throws and is `constexpr`, so a fixed pattern can be checked at compile
time. Its running time is bounded by the product of the two lengths whatever
the pattern contains.

Two predicates check well-formedness. `isValidAddress` accepts an address
that starts with `/`, whose parts are non-empty and no longer than
`kMaxAddressPartLength` bytes, and whose bytes are printable ASCII other than
`#*,?[]{}`. `isValidPattern` accepts a pattern that starts with `/`, is
printable ASCII, closes every `[` and `{`, and nests no brace list. `match`
validates neither argument: a malformed pattern matches nothing, and an
address is compared byte for byte.

With CMake, `find_package(oscpm)` or `add_subdirectory`, then link
`oscpm::oscpm`.

## Matching rules

A pattern and an address are split into parts on `/`. Both must have the
same number of parts, except where `//` applies, and every pattern part must
match the address part in the same position. Within a part:

| Pattern | Matches |
| --- | --- |
| `?` | any one byte |
| `*` | any run of zero or more bytes |
| `[abc]`, `[a-z]` | one byte from the list; `a-z` is an inclusive ASCII range |
| `[!abc]` | one byte not in the list |
| `{foo,bar}` | exactly one of the listed strings, taken literally |
| anything else | itself |

`//` matches zero or more whole parts: `/a//c` matches `/a/c` and `/a/b/c`.

Where the OSC 1.0 specification leaves a case open, oscpm does this:

- A wildcard, set or brace list never matches `/`: `/a*` does not match
  `/a/b`.
- A `-` first or last in a set is a member. `[a-c-e]` is the range `a-c`
  plus the members `-` and `e`.
- A reversed range such as `[z-a]` matches nothing. `[]` matches nothing;
  `[!]` matches any byte.
- An empty alternative matches the empty string: `{a,}` matches `a` or
  nothing.
- A `]` or `}` outside a set or brace list is a literal.
- `/a//c` does not match `/ab/c`; the part before `//` must match whole. A
  run of three or more slashes is one `//`. A trailing `/` is an empty part
  that only an address ending in `/` satisfies.
- Matching is by byte and case-sensitive.

## Building

Builds go through `CMakePresets.json`:

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja; `windows` uses Visual Studio 2022. The tests
use Catch2, fetched at configure time; `DEPENDENCIES.md` lists what is fetched
and why.

## Licence

zlib. See `LICENSE`.
