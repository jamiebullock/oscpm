# oscpm

oscpm is a header-only [Open Sound Control](https://opensoundcontrol.stanford.edu/spec-1_0.html)
address pattern matcher.

The main classes are a `Matcher` that supplies a match decision for every
pattern and address pair it receives, and an `AddressSpace` which dispatches registered callbacks
based on address and pattern matches.

## Integration

With CMake 3.25 or later, `add_subdirectory` on a checkout gives the target
`oscpm::oscpm`; `cmake --install` installs the header and a package config
for `find_package(oscpm)`. The tests are built only when oscpm is the
top-level project.

## Matching

```cpp
#include <oscpm/oscpm.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true

const oscpm::Pattern pattern("/synth/[1-3]/{freq,amp}");
pattern.valid();                 // false if the pattern is malformed
pattern.matches("/synth/2/amp"); // true

oscpm::Matcher matcher;
matcher.match("/synth/*/freq", "/synth/1/freq"); // compiles, matches, remembers
matcher.match("/synth/*/freq", "/synth/1/freq"); // a hash lookup
```

`match` compiles the pattern and tests it once. A `Pattern` compiles once
and is matched many times. A `Matcher` memoises the verdict of every pattern and
address pair: the first call for a pair compiles and matches, every later
call for the same pair is a hash lookup. The memo holds 65,536 pairs by
default, about 10 MB. This can be overridden by argument to the constructor.
The cache empties when it reaches the limit, so a working set larger than the limit
recompiles on every message.

## Address space

```cpp
oscpm::AddressSpace<Handler> methods;
methods.add("/synth/1/freq", setFrequency); // false if already registered
methods.remove("/synth/1/freq");            // false if not registered

const std::size_t matched = methods.dispatch(message.address(), [&](std::string_view address, Handler& handler)
    { handler(message); });
```

`dispatch` first looks the pattern up as an address: a pattern equal to a
registered address reaches that method by one hash lookup. Any other
pattern is put to the address space's `Matcher` for every method,
so a wildcard message costs one memoised match per
registered method.

## Specification compliance

oscpm implements the [OSC 1.0
specification](https://opensoundcontrol.stanford.edu/spec-1_0.html) and the
`//` operator OSC 1.1 took from XPath, which matches zero or more whole parts:
`/a//c` matches `/a/c` and `/a/b/c`, and `//` on its own matches every
address. Any run of two or more slashes is that operator, and so is a trailing
slash, so `/a/` matches `/a` and everything below it.

The specification asks only that a pattern begin with `/`. A pattern that does
not is malformed here, and a malformed pattern matches nothing.

Four departures from the specification remain:

- A negated class matches the part separator, so `/a[!x]b` matches `/a/b`,
  where the specification says no wildcard spans parts.
- A comma outside braces alternates the whole pattern rather than the part, so
  `/synth/1,/other` matches both `/synth/1` and `/other`.
- Inside braces, wildcards and classes are live and braces nest, so `/{a*,b}`
  matches `/ax` and `/{a,{b,c}}` matches `/b`. The specification calls the
  contents a list of strings and says nothing about either; other
  implementations take them literally.
- A lone `/` is malformed here, where the specification permits it. Since no
  legal address has an empty part, it matches nothing either way, so the two
  readings differ only in what `valid` reports.

## Guarantees and their limits

Allocation:

- `match` and `Pattern::matches` allocate on every call.
- `Matcher::match` allocates the first time it sees a pattern and address
  pair, and nothing on any later call for that pair until the cache fills and
  empties.
- `AddressSpace::dispatch` allocates nothing for a message to a registered
  address, and nothing for a wildcard message whose pairs are already cached.
  The tests assert both.

Time:

- Matching time is not bounded. The expensive shape is `*` separated by
  literals within one part: where the match runs to completion, `/*a*a*a*b`
  against a 200-byte part takes about three seconds and `/*a*a*a*a*b` over a
  minute.
- Builds against libc++ or the Microsoft standard library, the defaults on
  macOS and Windows, abandon a match that costs too much and report no match
  even where it would have matched. libstdc++, the default on Linux, runs it
  to completion instead. So a pattern that is merely slow on one platform can
  be answered wrongly on another.
- `dispatch` pays that cost once per registered method, so one such message
  against a thousand methods takes seconds to return nothing; later messages
  carrying the same pattern are cache hits.
- Accept patterns from elsewhere only under your own limit on their length and
  wildcard count.

Speed, over 1,000 registered methods on an Apple Silicon Mac: a message to a
registered address costs about 10 nanoseconds, a wildcard message whose pairs
are cached about 35 microseconds, and the first wildcard message carrying a new
pattern 1 to 4 milliseconds.

A `Matcher` and an `AddressSpace` are not safe to use from several threads at
once.

## Building

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja. The tests use doctest, fetched at
configure time. The build fails on a clang-format violation when oscpm is
the top-level project.

## Licence

zlib. See `LICENSE`.
