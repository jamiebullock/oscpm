# oscpm

Header-only C++17 OpenSoundControl (OSC) address pattern matching.

oscpm matches OSC address patterns against OSC addresses: the `?`, `*`,
`[...]` and `{a,b}` syntax of OSC 1.0, plus the `//` operator that OSC 1.1
took from XPath. It complements [oscpp](https://github.com/kaoskorobase/oscpp),
which reads and writes OSC packets but leaves address matching to the caller:
the address oscpp hands you is what oscpm matches. oscpm is usable on its own
and depends on nothing outside the standard library.

## Integration

With CMake 3.25 or later, any of these gives the target `oscpm::oscpm`:

- **FetchContent**, pinned to a release tag:

  ```cmake
  include(FetchContent)
  FetchContent_Declare(oscpm
      GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
      GIT_TAG        v0.2.6)
  FetchContent_MakeAvailable(oscpm)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

- **`find_package`** after `cmake --install`, which installs the headers and
  a package config with a version file:

  ```cmake
  find_package(oscpm 0.2 REQUIRED)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

- **`add_subdirectory`** on a checkout or a vendored copy. A source archive
  has no git history to read the version from, so pass
  `-DOSCPM_VERSION=<version>` when configuring it.

The tests and example are built by default only when oscpm is the top-level
project, and the fuzz target only on request, so a consumer compiles nothing
unless it turns them on.

## Matching

```cpp
#include <oscpm/oscpm.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true
oscpm::match("/synth/[1-3]/{freq,amp}", "/synth/2/amp"); // true
```

`match(pattern, address)` takes two `std::string_view`s. A pattern that is
matched many times is parsed once into a `Pattern` value:

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

## Address space

`oscpm/address_space.h` adds a small dispatcher: a set of methods, each a
well-formed address with a value, that an incoming pattern is fanned out to.

```cpp
#include <oscpm/address_space.h>

oscpm::AddressSpace<Handler> methods;
methods.add("/synth/1/freq", setFrequency); // Duplicate or a validateAddress fault
methods.remove("/synth/1/freq");            // NotFound or a validateAddress fault

const oscpm::DispatchResult result = methods.dispatch(message.address(), [&](std::string_view address, Handler& handler)
    { handler(message); });
if (result.error)
{
    result.error->kind; // the pattern did not parse and nothing was visited
}
```

`dispatch` is `Pattern::parse` followed by `lookup`, returning the number of
methods visited and the parse fault together; `lookup` takes an already
parsed `Pattern` for a pattern that is reused. Both visit every matching
method in bytewise address order; `forEach` visits them all. A literal pattern is a binary
search. Any other pattern is matched against every method, so a cold lookup
costs O(N) in the number of methods; by default the result is then memoised
until the next `add` or `remove`, so repeating the same pattern costs a hash
of its bytes. The memo is direct-mapped with `1 << CacheBits` entries, each
holding a pattern of up to `kMaxMemoPatternLength` bytes and up to
`InlineResults` results; a lookup that exceeds either limit is delivered in
full but not memoised. Its memory is
`(1 << CacheBits) * (kMaxMemoPatternLength + InlineResults * sizeof(std::size_t) + 24)`
bytes, about 200 KiB for the defaults of `CacheBits = 8` and
`InlineResults = 64`, allocated when the space is constructed.
`AddressSpace<T, false>` has no memo and costs nothing beyond its methods.
An address space is not safe to use from several threads at once.

`examples/dispatch.cpp` puts the two together with oscpp: it builds a bundle
with oscpp's client API, reads it back with the server API, fans each
message's address out to the registered methods through an address space,
and filters the incoming addresses through a stored pattern.

## Guarantees

`match`, `Pattern::parse`, `Pattern::matches`, `validatePattern`,
`validateAddress`, `AddressSpace::lookup`, `AddressSpace::dispatch` and
`AddressSpace::forEach`, the last three apart from whatever the visitor they
call does:

- allocate nothing, which the test suite asserts with a counting
  `operator new`;
- never throw, and are declared `noexcept` outside the address space;
- run in time bounded by the product of the pattern and address lengths,
  whatever the pattern contains: a part is matched by a reach-set
  simulation, never by backtracking over alternatives, and `//` keeps a
  single backtrack point;
- are `constexpr` outside the address space, so a fixed pattern is parsed,
  validated or matched at compile time.

`AddressSpace::add` and `remove` allocate. A libFuzzer target checks the
matcher, the validators and the pattern value against each other under
AddressSanitizer and UndefinedBehaviorSanitizer on every change.

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
use Catch2 and the example uses oscpp, both fetched at configure time;
`DEPENDENCIES.md` lists what is fetched and why. `-DOSCPM_BUILD_EXAMPLES=OFF`
skips the example and its fetch. `-DOSCPM_BUILD_FUZZERS=ON` adds a libFuzzer
target under AddressSanitizer and UndefinedBehaviorSanitizer, seeded from the
corpus; it needs an LLVM clang. `-DOSCPM_SANITIZE=ON` builds the tests under
the same sanitizers.

## Versioning

Every push to `develop` is tagged `vX.Y.Z`, and CMake reads the version from
the most recent tag. While the major version is 0 a minor release may change
the API, so the installed package config accepts only the same minor
version; from 1.0 it accepts the same major. A `(MINOR)` or `(MAJOR)` marker
in a commit subject moves that component; every other push moves the patch.

## Licence

zlib. See `LICENSE`.
