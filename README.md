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

A pattern that is matched many times is parsed once into a `Pattern` value:

```cpp
constexpr auto parsed = oscpm::Pattern::parse("/synth/*/{freq,amp}");
static_assert(parsed);
static_assert(parsed.pattern().matches("/synth/12/amp"));

if (const auto result = oscpm::Pattern::parse(text))
{
    const oscpm::Pattern& pattern = result.pattern();
    pattern.matches(message.address()); // any std::string_view
}
else
{
    result.error(); // an Error and a byte offset
}
```

`Pattern::parse` returns a `ParseResult` holding either the pattern or the
`ParseError` that stopped it parsing. A `Pattern` is a trivially copyable
view of the bytes it was parsed from, which must outlive every use;
`text()` returns them and `isLiteral()` says whether the pattern contains no
`*`, `?`, `[`, `{` or slash run and no part longer than
`kMaxAddressPartLength`, and so matches only an address equal to its text,
which `matches` then decides with a single comparison. `match` is `parse`
followed by `matches`: a malformed pattern matches nothing, and `parse` says
why.

Two validators report the first fault in their input as an `Error` and a
byte offset, or nothing when it is well-formed:

```cpp
if (const auto fault = oscpm::validatePattern("/synth/[1-3")) // UnterminatedClass at 7
{
    std::printf("%s at %zu\n", oscpm::toString(fault->kind), fault->offset);
}
oscpm::validateAddress("/synth/1/"); // TrailingSlash at 8
```

`validatePattern` rejects only a pattern that does not start with `/`
(`MissingLeadingSlash`) or leaves a `[` or `{` unclosed within its part
(`UnterminatedClass`, `UnterminatedBraces`); every other pattern parses.
`validateAddress` applies the OSC address rules: a leading `/`
(`MissingLeadingSlash`), no trailing `/` (`TrailingSlash`), no empty part
(`EmptyPart`), only printable ASCII other than `#*,?[]{}` and space
(`IllegalByte`), and no part longer than `kMaxAddressPartLength` bytes
(`PartTooLong`). `match` rejects a pattern `validatePattern` faults and never
validates the address, which is compared byte for byte.

With CMake 3.25 or later, `find_package(oscpm)` or `add_subdirectory`, then
link `oscpm::oscpm`.

## Matching rules

A pattern and an address are split into parts on `/`. Both must have the
same number of parts, except where `//` applies, and every pattern part must
match the address part in the same position. Within a part:

| Pattern | Matches |
| --- | --- |
| `?` | any one byte |
| `*` | any run of zero or more bytes |
| `[abc]`, `[a-z]` | one byte from the class; `a-z` is an inclusive ASCII range |
| `[!abc]` | one byte not in the class |
| `{foo,bar}` | exactly one of the listed strings, taken literally |
| anything else | itself |

`//` matches zero or more whole parts: `/a//c` matches `/a/c` and `/a/b/c`.

Where the OSC 1.0 specification leaves a case open, oscpm does this:

- A wildcard, class or brace list never matches `/`: `/a*` does not match
  `/a/b`.
- A `-` between two characters is a range, so `[--a]` is the range `-`..`a`;
  first or last it is a member. `[a-c-e]` is the range `a-c` plus the
  members `-` and `e`. A reversed range such as `[z-a]` matches nothing.
- `[]` matches nothing; `[!]` matches any byte. Only a leading `!` negates.
- Inside `{` and `}` every byte is literal, `,` always delimits and the
  first `}` closes: `{a,{b,c}}` is the members `a`, `{b` and `c` followed by
  a literal `}`. An empty member matches the empty string: `{a,}` matches
  `a` or nothing.
- A `]`, `}` or `,` outside its construct, a `#`, a space and any byte
  outside printable ASCII are literals; since no address can contain them,
  they match nothing where they stand, and a brace list member holding one
  is dead while the other members stay live.
- A run of two or more slashes anywhere is one `//`: leading, `//a` is `a`
  at any depth; between parts, `/a///b` is `/a//b`; trailing, `/a//` is
  `/a` and every descendant of it, and a bare `//` matches every address.
  The part before `//` must match whole: `/a//c` does not match `/ab/c`.
- A single trailing `/` is an empty part that only an address ending in `/`
  satisfies, so `/a/` does not match `/a`. The bare `/` likewise matches
  only the address `/`.
- Matching is by byte and case-sensitive.

[`corpus/matching.txt`](corpus/matching.txt) is the executable record of
these rules: one case per line, replayed by the test suite, in a plain-text
format other implementations can reuse.

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
