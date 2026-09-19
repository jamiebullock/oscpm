# oscpm

[![CI](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml/badge.svg)](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml)

**oscpm** is a header-only C++17 library for OSC address pattern matching.
It implements the OSC 1.0 wildcards (`?`, `*`, `[...]`, `{...}`) and the
OSC 1.1 descendant operator (`//`), reports a malformed pattern distinctly
from a non-match, and does no heap allocation and throws no exceptions on
the matching path, so it can run inside an audio callback.

It complements [oscpp](https://github.com/kaoskorobase/oscpp), which parses
and builds OSC packets but leaves address pattern matching to the caller.
An optional adapter header takes an oscpp server message directly; the core
accepts plain `std::string_view` and has no dependencies, so it also works
with JUCE, oscpack, liblo or a hand-rolled parser.

oscpm gives you two things:

- **A matcher.** Parse and validate a `Pattern` once, then test it against
  any number of addresses.
- **An address space.** Register methods at literal addresses with a value
  of your own type, then look up every method a pattern matches.

## Adding oscpm to a project

oscpm needs CMake 3.14 or later and a C++17 compiler. The core target is
`oscpm::oscpm`.

**With FetchContent:**

```cmake
include(FetchContent)
FetchContent_Declare(oscpm
    GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
    GIT_TAG        0.1.0)
FetchContent_MakeAvailable(oscpm)

target_link_libraries(myapp PRIVATE oscpm::oscpm)
```

**As a subdirectory** (a git submodule or a vendored copy):

```cmake
add_subdirectory(third_party/oscpm)
target_link_libraries(myapp PRIVATE oscpm::oscpm)
```

**From an install:**

```shell
cmake -S oscpm -B oscpm-build -DOSCPM_BUILD_TESTS=OFF -DOSCPM_BUILD_EXAMPLES=OFF
cmake --install oscpm-build --prefix /some/prefix
```

```cmake
find_package(oscpm 0.1 CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE oscpm::oscpm)
```

The install is the headers and a CMake package config; there is no library
file. The version check is semantic: while the major version is 0 the
package satisfies a request for the same minor version, from 1.0 the same
major version.

**Without CMake:** copy the `include` directory onto your include path.
There is nothing to compile.

### The oscpp adapter target

`oscpm::oscpp` is a second interface target that carries the adapter header
and links oscpp. It exists only when oscpp is located at configure time,
and the core target never depends on it. oscpm looks for oscpp in this
order:

1. A target named `oscpp::oscpp` that your project already created, for
   example with its own `FetchContent` call before `add_subdirectory(oscpm)`.
2. An installed oscpp package, through `find_package(oscpp CONFIG)`.
3. With the option `OSCPM_FETCH_OSCPP` on, a `FetchContent` pull of oscpp
   1.0.0. The option is on when oscpm is the top-level project and off
   otherwise.

```cmake
target_link_libraries(myapp PRIVATE oscpm::oscpp)  # brings oscpm::oscpm and oscpp::oscpp
```

An installed oscpm exports the adapter target too, when oscpp was located
while oscpm was configured. `find_package(oscpm)` then provides
`oscpm::oscpp` if oscpp can be found at your configure time, as a target
you already created or as an installed package, and omits it otherwise.
Ask for it with `find_package(oscpm CONFIG REQUIRED COMPONENTS oscpp)` to
make its absence an error rather than a missing target.

## The three headers

| Header | Provides | Depends on |
| --- | --- | --- |
| `<oscpm/pattern.hpp>` | `Pattern`, `ParseResult`, `match`, `validateAddress`, `MatchResult`, `Error`, `ErrorKind` | the standard library |
| `<oscpm/address_space.hpp>` | `AddressSpace<T>`, `Method<T>`, `LookupCount` | `pattern.hpp` |
| `<oscpm/oscpp.hpp>` | `parsePattern`, `lookup` for an `OSCPP::Server::Message`, `LookupResult` | `address_space.hpp` and `<oscpp/server.hpp>` |

Include only what you use. The matcher alone is enough for a project that
keeps its own table of addresses.

## Matching a pattern

```cpp
#include <oscpm/pattern.hpp>

#include <cstdio>

void matcher()
{
    // Validate once. The Pattern is a view of these bytes, so they must
    // outlive it: a string literal lives forever, a network buffer does not.
    const oscpm::ParseResult parsed = oscpm::Pattern::parse("/synth/[1-4]/{freq,amp}");
    if (!parsed.ok()) {
        std::printf("malformed: %s at byte %zu\n", oscpm::toString(parsed.error().kind),
                    parsed.error().offset);
        return;
    }
    const oscpm::Pattern pattern = parsed.pattern();

    // Match as often as you like. Each call is a single pass with no heap.
    switch (pattern.matches("/synth/2/freq")) {
    case oscpm::MatchResult::Match:
        break; // Reached.
    case oscpm::MatchResult::NoMatch:
        break; // Well-formed, but not this address.
    case oscpm::MatchResult::Malformed:
        break; // The address was malformed; validateAddress says where.
    }

    // One call for the simple case: parse, then match.
    const oscpm::MatchResult deep = oscpm::match("//gain", "/mixer/bus/3/gain");
    (void)deep; // Match

    // Diagnose a pattern that arrived from the network.
    const oscpm::ParseResult bad = oscpm::Pattern::parse("/synth/[1/freq");
    std::printf("%s at byte %zu\n", oscpm::toString(bad.error().kind), bad.error().offset);
    // UnterminatedCharacterClass at byte 7
}
```

`ParseResult` holds either a `Pattern` or an `Error`. `Error` carries an
`ErrorKind` and the zero-based byte offset of the byte that made the input
malformed; `toString(kind)` gives the enumerator's name for a log line.

## An address space

`AddressSpace<T>` stores a value of your choosing at each registered
address. `T` need only be move-constructible, so a `std::unique_ptr`, a
function pointer or a capturing lambda all work.

```cpp
#include <oscpm/address_space.hpp>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string_view>

using Handler = void (*)(float);

void setFrequency(float hz) { std::printf("freq %g\n", static_cast<double>(hz)); }
void setAmplitude(float amp) { std::printf("amp %g\n", static_cast<double>(amp)); }

void addressSpace()
{
    // Setup thread: registration may allocate.
    oscpm::AddressSpace<Handler> space;
    space.add("/synth/1/freq", &setFrequency);
    space.add("/synth/1/amp", &setAmplitude);
    space.add("/synth/2/freq", &setFrequency);
    space.add("/synth/2/amp", &setAmplitude);

    // add reports why it refused: a malformed address or a duplicate.
    if (const std::optional<oscpm::Error> error = space.add("/synth/*/freq", &setFrequency)) {
        std::printf("refused: %s at byte %zu\n", oscpm::toString(error->kind), error->offset);
        // IllegalCharacter at byte 7: addresses are literal, only patterns carry wildcards
    }

    // Audio thread: a Lookup never allocates. The visitor is called once per
    // matching method, in address order, and the count comes back.
    const oscpm::ParseResult parsed = oscpm::Pattern::parse("/synth/*/freq");
    const std::size_t reached =
        space.lookup(parsed.pattern(), [](std::string_view address, Handler& handler) {
            std::printf("%.*s: ", static_cast<int>(address.size()), address.data());
            handler(440.0f);
        });
    if (reached == 0) {
        // Well-formed, but no method matched.
    }

    // Or collect into your own storage when a visitor is inconvenient.
    oscpm::Method<Handler> methods[8];
    const oscpm::LookupCount count = space.lookup(parsed.pattern(), methods, 8);
    for (std::size_t i = 0; i < count.stored; ++i) {
        (*methods[i].value)(220.0f);
    }
    // count.matched > count.stored only if the storage was too small.

    // Walk every method in address order, for an address space browser or
    // an OSCQuery-style reply.
    space.forEach([](std::string_view address, Handler&) {
        std::printf("%.*s\n", static_cast<int>(address.size()), address.data());
    });

    space.remove("/synth/2/amp"); // NotFound if nothing is registered there.
}
```

An address may be both a container and a method: `/synth` and
`/synth/freq` coexist. The `T&` and the address a visitor receives stay
valid until that method is removed or the address space is destroyed;
later registrations never move them.

## With oscpp

The adapter header wraps the address an `OSCPP::Server::Message` carries
and calls the core, so nothing is converted by hand at the call site.

```cpp
#include <oscpm/oscpp.hpp>

#include <cstdio>
#include <string_view>

using Handler = void (*)(std::string_view address, OSCPP::Server::ArgStream args);

void handleMessage(const oscpm::AddressSpace<Handler>& space,
                   const OSCPP::Server::Message& message)
{
    const oscpm::LookupResult result =
        oscpm::lookup(space, message, [&](std::string_view address, const Handler& handler) {
            handler(address, message.args());
        });

    if (result.error) {
        // The message's address pattern was malformed; nothing was visited.
        std::printf("malformed: %s at byte %zu\n", oscpm::toString(result.error->kind),
                    result.error->offset);
    } else if (result.matched == 0) {
        // Well-formed, but no method matched.
    }
}

// Or take just the Pattern and use any core overload. It views the packet's
// bytes, exactly as the Message does.
void justThePattern(const OSCPP::Server::Message& message)
{
    const oscpm::ParseResult parsed = oscpm::parsePattern(message);
    if (parsed.ok()) {
        (void)parsed.pattern().matches("/synth/1/freq");
    }
}
```

`examples/oscpp_dispatch.cpp` is a complete program that builds a bundle
with oscpp's client API, parses it with the server API and looks each
message up through the adapter. It has no networking dependency and prints
which methods each message reached; the `//gain` message reaches both
`/master/gain` and `/synth/1/gain`, and a malformed `/synth/[1/freq` is
reported rather than silently dropped.

## Guarantees

**No allocation on the matching path.** `Pattern::parse`, `matches`,
`match`, `validateAddress`, `AddressSpace::lookup`, `forEach` and both
adapter functions never touch the heap. The test suite asserts this with a
counting `operator new`. Only `AddressSpace::add` allocates, at least once
per new part of an address, and it is meant for the setup thread.

**No exceptions.** Nothing in oscpm throws. Every failure is a result: a
`ParseResult` that is not `ok()`, a `MatchResult::Malformed`, or a
`std::optional<Error>` from `add` and `remove`. Two things can still unwind
through oscpm because they are not oscpm's: `add` can propagate
`std::bad_alloc` from the standard containers it grows, and a visitor you
pass to `lookup` or `forEach` may throw on its own. oscpp's parser throws
its own exceptions when a packet is truncated; the adapter runs after oscpp
has already constructed the `Message`, so it inherits none of them.

**Bounded work.** Matching a part is a set simulation, not a backtracking
search, so a hostile pattern such as `{a,}{a,}{a,}…` costs no more than
the product of the two part lengths. The price is a limit on the untrusted
side: a pattern part longer than `oscpm::maxPatternPartLength` (8191
bytes) is reported as `PartTooLong`. Addresses have no limit.

**A `Pattern` does not own its bytes.** `Pattern::parse` takes a
`std::string_view` and the resulting `Pattern` is a trivially copyable view
of those same bytes. You own them for as long as any `Pattern` parsed from
them is in use. This is the same contract oscpp applies to its message
views, and it is what keeps matching allocation-free. Parse a pattern from
a `std::string` you are about to destroy and you have a dangling view. A
pattern parsed from an oscpp message is valid exactly as long as the packet
buffer is.

**Not thread-safe for concurrent mutation.** Register on the setup thread,
look up on any one thread at a time. Mutating an address space while
another thread runs a lookup is unsupported.

## Pattern syntax

| Syntax | Matches | Notes |
| --- | --- | --- |
| `a` | the byte `a` | byte-wise and case-sensitive; no escaping; `#` and space are illegal |
| `?` | exactly one byte within a part | never `/` |
| `*` | zero or more bytes within a part | never `/` |
| `[abc]`, `[a-z]` | one byte from the set | ranges in byte order; `-` is literal when first or last |
| `[!abc]` | one byte not in the set | |
| `{foo,bar}` | exactly one of the members, literally | a member may be empty; no nesting, no `[` inside braces |
| `//` | zero or more whole parts | leading or between parts; trailing `//` and `///` are malformed |

Every address and pattern begins with `/` and has at least one non-empty
part; the bare root, a trailing `/` and an empty part are malformed.

The OSC specifications are silent on many of these details: whether `//`
means zero or one-or-more parts, whether `{a,}` matches the empty string,
whether `[a-]` is a range, whether `*` inside braces is a wildcard. Every
such decision oscpm makes is recorded as executable cases in
[`corpus/matching.txt`](corpus/matching.txt), one case per line, grouped
by rule and by decision. The corpus is plain text so that other OSC
implementations can run it too, and it is the authoritative record of
oscpm's interpretation. The reasoning behind the larger decisions is in
[`docs/adr/`](docs/adr/).

## Versioning

oscpm follows [semantic versioning](https://semver.org). While the major
version is 0 the public API may change in a minor release; from 1.0 it
changes only in a major release. Each release is a git tag named after the
version, so `GIT_TAG 0.1.0` above pins one.

`<oscpm/version.hpp>`, which every other header includes, defines
`OSCPM_VERSION_MAJOR`, `OSCPM_VERSION_MINOR` and `OSCPM_VERSION_PATCH` as
integers for `#if`, and `OSCPM_VERSION` as the same value in a string for a
log line. The CMake project version is checked against them at configure
time, so they cannot drift apart.

## Naming

Everything lives in the lowercase namespace `oscpm`. Types are PascalCase
(`Pattern`, `AddressSpace`, `ErrorKind`) and functions are camelCase
(`parse`, `matches`, `validateAddress`, `forEach`), so code that mixes
oscpm and oscpp reads as one library. The vocabulary in identifiers and
comments follows [`CONTEXT.md`](CONTEXT.md): an *address* is literal, a
*pattern* may carry wildcards, a *part* is one `/`-delimited component, a
*method* is a registered address with its value, and a *lookup* finds every
method a pattern matches.

## Building and testing

```shell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests use Catch2 v3 and, for the adapter, oscpp, both pulled with
`FetchContent`. oscpp's own build files need CMake 3.24, so building oscpm's
tests and example needs that too; consuming oscpm needs only 3.14. The
conformance corpus is replayed against both the matcher and a populated
address space.

Two of the tests build `tests/consumer`, a separate CMake project that
links both targets, once against a fresh install of oscpm and once through
`FetchContent`, so that both routes are known to deliver what this README
promises. `-DOSCPM_WARNINGS_AS_ERRORS=ON` turns the strict warning level
the tests, example and fuzz target are built at into errors; CI builds
that way on Apple Clang, GCC and MSVC, and fuzzes for a minute under the
sanitizers on every push.

### Fuzzing

A libFuzzer target feeds arbitrary bytes to `Pattern::parse`, `matches`,
`match` and `validateAddress` under AddressSanitizer and
UndefinedBehaviorSanitizer, and aborts if the API contradicts itself. It
is off by default and needs an LLVM clang, since Apple clang, GCC and MSVC
ship no libFuzzer. On macOS use Homebrew's `llvm`; LLVM 20's
AddressSanitizer hangs at startup on macOS 26, where `llvm@22` works. To
build the target and fuzz for ten minutes:

```shell
cmake -S . -B build-fuzz -DOSCPM_BUILD_FUZZERS=ON -DOSCPM_BUILD_TESTS=OFF -DOSCPM_BUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" && cmake --build build-fuzz && build-fuzz/fuzz/oscpm_pattern_fuzz -max_total_time=600 -max_len=16384 build-fuzz/fuzz/corpus build-fuzz/fuzz/seeds
```

A fuzz input is a pattern, a newline, and an address. The seed corpus is
generated at build time from `corpus/matching.txt`, one seed per case, so
the fuzzer starts from every shape the corpus knows, and what it discovers
accumulates in `build-fuzz/fuzz/corpus` across runs. `-max_len` is raised
above libFuzzer's 4096-byte default so that a part can exceed
`maxPatternPartLength`. A crash or sanitizer report writes a `crash-*`
file into the current directory; its contents are a corpus case waiting
to be added, and any such finding is a bug in the scanner. With the tests
on, `ctest` checks the seed corpus and runs every seed through the harness
once.

## License

zlib. See [`LICENSE`](LICENSE).
