# oscpm

Header-only C++17 OpenSoundControl (OSC) address pattern matching.

[![CI](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml)
[![Latest tag](https://img.shields.io/github/v/tag/jamiebullock/oscpm?sort=semver&label=tag)](https://github.com/jamiebullock/oscpm/tags)
[![Licence](https://img.shields.io/github/license/jamiebullock/oscpm?label=licence)](LICENSE)

oscpm matches OSC address patterns against addresses and dispatches them to
methods through an address space. It supports the
[OSC 1.0](https://opensoundcontrol.stanford.edu/spec-1_0.html) matching
syntax, and the proposed `//` operator from
[OSC 1.1](https://opensoundcontrol.stanford.edu/spec-1_1.html).

It is designed to complement [oscpp](https://github.com/kaoskorobase/oscpp),
which reads and writes OSC packets and leaves matching and dispatch to the
caller. oscpm does not depend on oscpp, or on anything outside the standard
library.

## Example

```cpp
#include <oscpm/address_space.h>
#include <oscpp/server.hpp>

#include <functional>

using Handler = std::function<void(const OSCPP::Server::Message&)>;

void setFrequency(const OSCPP::Server::Message& message)
{
    synth.setFrequency(OSCPP::Server::ArgStream(message.args()).float32());
}

oscpm::AddressSpace<Handler> methods;
methods.add(oscpm::Address::parse("/synth/freq").address(), setFrequency);

// for each OSCPP::Server::Message received
methods.invoke(message.address(), message);
```

## Matching
In the simple case a match can be checked by comparing two `std::string_view`:

```cpp
#include <oscpm/pattern.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true (OSC 1.1 syntax)
oscpm::match("/synth/[1-3]/{freq,amp}", "/synth/2/amp"); // true
oscpm::match("/synth/*/freq", "/1/synth/freq"); // false
```

## Patterns
A pattern that will be matched many times can be validated once and kept as a `Pattern` object. This has the following advantages:
- A malformed pattern is reported as an `Error`so the caller can distinguish an invalid pattern from a non-match
- For `constexpr` patterns, validity can be checked at compile time
- `Pattern::matches()` skips validation on every call, and for a literal pattern it is a single byte comparison

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
    oscpm::toString(result.error()); // UnterminatedClass for "/synth/[1-3"
}
```

## Addresses
Similar to `Pattern`, `Address` is a validated OSC address:

```cpp
#include <oscpm/address.h>

constexpr auto parsed = oscpm::Address::parse("/synth/1/freq");
static_assert(parsed);

if (const auto result = oscpm::Address::parse(presetEntry))
{
    methods.add(result.address(), handler);
}
else
{
    oscpm::toString(result.error()); // TrailingSlash for "/synth/1/"
}
```

`Address::parse` returns an `Address::ParseResult` holding either the
address or the `Error` that stopped it parsing, and rejects an address only
for one of the faults under [Malformed addresses](#malformed-addresses). An
`Address` is a view of its text, which must outlive it unchanged: the
validation holds for the bytes that were parsed, and `AddressSpace::add`
registers whatever the view reads at the time. A build without `NDEBUG`
asserts in `add` and `remove` when those bytes no longer parse as an
address. It is what an address space
registers methods under, so an application that keeps
addresses of its own, in a document or a preset, parses each one where it
enters and stores text it knows to be well formed.

## Address space

`AddressSpace<T>` is a set of methods, each a well-formed address with a
value of type `T`, that an incoming pattern is dispatched to. `T` is
whatever the application needs to invoke a method: a handler, as in the
opening example, a parameter, as here, an index or a struct.

```cpp
#include <oscpm/address_space.h>

struct Parameter
{
    float value = 0.0f;
};

oscpm::AddressSpace<Parameter> parameters;
parameters.add(oscpm::Address::parse("/synth/1/freq").address(), Parameter { 440.0f });
parameters.add(oscpm::Address::parse("/synth/2/freq").address(), Parameter { 220.0f });

const oscpm::DispatchResult result = parameters.dispatch("/synth/*/freq", [](std::string_view, Parameter& parameter)
    { parameter.value = 550.0f; });
result.matched; // 2, the number of methods visited
result.error; // empty; the parse fault when the pattern was malformed and nothing was visited

const Parameter* parameter = parameters.find("/synth/1/freq"); // value is 550
parameters.find("/synth/3/freq"); // nullptr
```

- `add(address, value)` registers `value` under an `Address` and returns
  false when the address is already registered; `remove(address)` returns
  false when it is not.
- `dispatch(pattern, visitor)` parses the pattern and calls
  `visitor(address, value)` for every method it matches, in bytewise address
  order.
- `invoke(pattern, args...)` is `dispatch` with a visitor that calls
  `value(args...)`, for a space whose values are handlers, as in the opening
  example. Every handler is called with the same `args` objects, which
  `invoke` does not move from.
- `find(address)` returns a pointer to the value registered under one
  address, or null when there is none. The pointer is valid until the next
  `add` or `remove`.
- `forEach(visitor)` visits every method; `size()` counts them.
- A visitor, or a handler called by `invoke`, may call `dispatch`, `invoke`
  and `find` on the space that called it, and copy it, but must not add or
  remove methods, move from it or assign to it. To change the space in
  response to a message, collect the changes and apply them once the call
  has returned. A build without `NDEBUG` asserts when one adds or removes.
- An address space is not safe to use from several threads at once.

`AddressSpace<T, Memo, CacheBits, InlineResults>` keeps the result of a
dispatch until the next `add` or `remove`:

| Parameter | Default | Meaning |
| --- | --- | --- |
| `Memo` | `true` | whether results are kept |
| `CacheBits` | `8` | `1 << CacheBits` patterns are kept |
| `InlineResults` | `1024` | the most methods a kept result lists |

A pattern longer than `kMaxMemoPatternLength` or a result larger than
`InlineResults` is delivered in full but not kept. The default memo takes
about 1.1 MiB, allocated when the space is constructed;
`AddressSpace<T, true, 6, 64>` takes about 34 KiB and `AddressSpace<T, false>`
none.

## Matching rules

A pattern and an address are split into parts on `/`. Both must have the
same number of parts, except where `//` applies, and every pattern part must
match the address part in the same position.

| Syntax | Meaning | Example | Matches | Does not match |
| --- | --- | --- | --- | --- |
| `?` | any one byte | `/synth/?` | `/synth/1` | `/synth/10` |
| `*` | any run of zero or more bytes | `/synth*` | `/synth`, `/synth1` | `/synth/1` |
| `[abc]`, `[a-z]` | one byte from the class; `a-z` is an inclusive ASCII range | `/synth/[1-3]` | `/synth/2` | `/synth/4` |
| `[!abc]` | one byte not in the class | `/synth/[!1-3]` | `/synth/4` | `/synth/2` |
| `{foo,bar}` | exactly one of the listed strings, taken literally | `/synth/{freq,amp}` | `/synth/amp` | `/synth/pan` |
| `//` | zero or more whole parts | `/a//c` | `/a/c`, `/a/b/c` | `/ab/c` |
| anything else | itself | `/synth` | `/synth` | `/Synth` |

Where the
[OSC 1.0 specification](https://opensoundcontrol.stanford.edu/spec-1_0.html)
leaves a case open, oscpm does the following.

### Parts and slashes

- A wildcard, class or brace list never matches `/`: `/a*` does not match
  `/a/b`.
- A run of two or more slashes anywhere is one `//`, and the part before it
  must match whole:

  | Pattern | Matches |
  | --- | --- |
  | `//a` | `a` at any depth: `/a`, `/x/a`, `/x/y/a` |
  | `/a///b` | whatever `/a//b` matches |
  | `/a//` | `/a` and every address below it |
  | `//` | every address |

- A single trailing `/` is an empty part that only an address ending in `/`
  satisfies, so `/a/` does not match `/a`. The bare `/` likewise matches
  only the address `/`.

### Classes

A `-` between two characters is a range; first or last it is a member. Only
a leading `!` negates.

| Class | Matches |
| --- | --- |
| `[--a]` | one byte in the range `-` to `a` |
| `[a-]`, `[-a]` | `a` or `-` |
| `[a-c-e]` | one byte in the range `a` to `c`, or `-`, or `e` |
| `[z-a]` | nothing: a reversed range is empty |
| `[]` | nothing |
| `[!]` | any one byte |
| `[a!]` | `a` or `!` |

### Brace lists

- Inside `{` and `}` every byte is literal, `,` always delimits and the
  first `}` closes: `{a,{b,c}}` is the members `a`, `{b` and `c` followed by
  a literal `}`.
- An empty member matches the empty string: `{a,}` matches `a` or nothing.

### Other bytes

- A `]`, `}` or `,` outside its construct, a `#`, a space and any byte
  outside printable ASCII is a literal that matches only itself. No
  well-formed address contains one, so a part holding one matches no
  well-formed address. In a brace list only the member holding it is
  affected: `{a b,c}` still matches `c`.
- Matching is by byte and case-sensitive, and the address is not validated.

### Malformed patterns

`Pattern::parse` rejects a pattern only for one of these faults:

| Fault | `Error` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a `[` with no `]` before the next `/` | `UnterminatedClass` |
| a `{` with no `}` before the next `/` | `UnterminatedBraces` |
| a wildcard, class, brace list or `//` in a pattern longer than `kMaxPatternLength` | `PatternTooLong` |

Every other pattern parses. A rejected pattern matches nothing: `match`
returns false, and `AddressSpace::dispatch` and `invoke` reach no method.

### Malformed addresses

`Address::parse` rejects an address only for one of these faults:

| Fault | `Error` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a final `/`, including the bare `/` | `TrailingSlash` |
| two adjacent slashes | `EmptyPart` |
| a byte outside printable ASCII, or one of `space`, `#`, `*`, `,`, `?`, `[`, `]`, `{`, `}` | `IllegalByte` |
| a part longer than `kMaxAddressPartLength` | `PartTooLong` |

Every well-formed address also parses as a literal pattern that matches
only itself.

[`corpus/matching.txt`](corpus/matching.txt) is the executable record of
these rules: one case per line, replayed by the test suite, in a plain-text
format other implementations can reuse.

## Guarantees

`match`, `Pattern::parse`, `Pattern::matches`, `Address::parse` and
`AddressSpace::find`:

- allocate nothing, which the test suite asserts with a counting
  `operator new`;
- never throw, and are declared `noexcept`;
- run in time bounded by the product of the pattern and address lengths,
  whatever the pattern contains;
- are `constexpr` outside the address space, so a fixed pattern is parsed
  or matched at compile time.

`AddressSpace::dispatch`, `invoke` and `forEach` allocate nothing and never
throw, apart from whatever the visitor or handler they call does; they are
not declared `noexcept`. A dispatch takes at most the time bound above for
each registered method. `AddressSpace::add` and `remove` allocate.

A libFuzzer target checks the matcher, the pattern value and
`Address::parse` against each other under AddressSanitizer and
UndefinedBehaviorSanitizer on every change.

## Performance

- Processor: Apple M4
- Operating system: macOS 26.5.2
- Compiler: AppleClang 21.0.0.21000101, `-O3 -DNDEBUG`
- oscpm: v0.3.6, measured 2026-09-24
- Method: median wall-clock time of 10 repetitions; dispatch is into a space
  of 1,856 methods

**Repeated dispatch of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 5.51 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq`, matching 128 methods | 50.1 |
| `//freq`, matching 512 methods | 154 |
| One message of a repeating stream of 64 | 10.4 |

**First dispatch of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 5.12 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq` | 88,600 |
| `//freq` | 32,000 |

**One pattern against one address**

| Operation | Median (ns) |
|---|---:|
| `Pattern::matches`, literal | 1.3 |
| `Pattern::matches`, wildcards | 100 |
| `oscpm::match` (parse and match), wildcards | 125 |

**Hostile patterns**

| Pattern | Median (us) |
|---|---:|
| The slowest pattern of 1024 bytes (`list-alternatives`) | 5,000 |
| A 64 KB wildcard pattern, rejected | 2.96 |

- A repeated message costs one hash lookup plus one visitor call per matched
  method. A literal address costs the same the first time.
- The first dispatch of any other pattern tests every registered method, so
  its cost grows with the size of the space.
- A wildcard pattern longer than `kMaxPatternLength` is rejected without
  being matched, which bounds the cost of a hostile pattern.
- The times are from one machine; the ratios between rows carry across
  machines better than the absolute figures.

### Running the benchmarks

```
cmake --preset bench
cmake --build --preset bench
build/bench/bench/oscpm_bench
```

`python3 bench/compare.py report` builds the benchmarks and prints the setup
and tables above.

## Integration

CI builds and tests oscpm with GCC and Clang on Linux, AppleClang on macOS
and MSVC on Windows.

With CMake 3.26 or later, any of these gives the target `oscpm::oscpm`:

- **FetchContent**, pinned to one of the
  [release tags](https://github.com/jamiebullock/oscpm/tags):

  ```cmake
  include(FetchContent)
  FetchContent_Declare(oscpm
      GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
      GIT_TAG        <tag>)
  FetchContent_MakeAvailable(oscpm)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

- **`find_package`** after `cmake --install`, which installs the headers and
  a package config with a version file:

  ```cmake
  find_package(oscpm REQUIRED)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

  While the major version is 0, a version passed to `find_package` is
  satisfied only by an install of the same minor version; from 1.0, by one
  of the same major version.

- **`add_subdirectory`** on a checkout or a vendored copy. A source archive
  has no git history to read the version from, so pass
  `-DOSCPM_VERSION=<version>` when configuring it.

Without CMake, put `include/` on the include path.

`oscpm/pattern.h` holds the matcher, `oscpm/address.h` the address value,
`oscpm/address_space.h` the dispatcher and `oscpm/error.h` the faults they
report; `oscpm/oscpm.h` includes all four. Nothing under `oscpm/detail/` is
public API.

## Building

Builds go through `CMakePresets.json`:

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja, `windows` uses Visual Studio 2022 and
`bench` is `release` with the benchmarks. `mutation` is `release` with the
tests compiled under clang-18 with the
[Mull](https://github.com/mull-project/mull) plugin, and its build runs them
once per mutant; it needs Mull for LLVM 18 on Linux. `DEPENDENCIES.md` lists
what is fetched at configure time and why.

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | builds the tests and fetches doctest; `OFF` registers nothing with CTest. Ignored when oscpm is a subproject, which never builds the tests |
| `OSCPM_BUILD_EXAMPLES` | `ON` at the top level, `OFF` as a subproject | builds the example and fetches oscpp |
| `OSCPM_CHECK_FORMAT` | `ON` at the top level, `OFF` as a subproject | fails the build on a clang-format violation; needs clang-format |
| `OSCPM_BUILD_BENCHMARKS` | `OFF` | builds the benchmarks and fetches Google Benchmark; the `bench` preset sets it |
| `OSCPM_BUILD_FUZZERS` | `OFF` | builds a libFuzzer target under AddressSanitizer and UndefinedBehaviorSanitizer, seeded from the corpus; needs an LLVM clang |
| `OSCPM_SANITIZE` | `OFF` | builds the tests under the same sanitizers |
| `OSCPM_MULL_PLUGIN` | empty | the Mull IR plugin to compile the tests with; adds the `mutation` target, which fails when the mutation score is below `OSCPM_MUTATION_THRESHOLD`. The `mutation` preset sets it |

## Versioning

oscpm follows [Semantic Versioning](https://semver.org/). Each release is a
`vX.Y.Z` [tag](https://github.com/jamiebullock/oscpm/tags).

## Licence

zlib. See [`LICENSE`](LICENSE).
