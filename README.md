# oscpm

oscpm is a header-only [Open Sound Control](https://opensoundcontrol.stanford.edu/spec-1_0.html)
address pattern matcher.

The main classes are a `Pattern`, an address pattern compiled once and
matched many times, and an `AddressSpace` which dispatches registered
callbacks based on address and pattern matches.

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
```

`match` compiles the pattern and tests it once. A `Pattern` compiles once
and is matched many times.

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
pattern is compiled once and matched against every method the first time
it is dispatched, and the methods it reaches are remembered until the next
`add` or `remove`, so dispatching it again is one hash lookup. The address
space remembers up to 4,096 patterns and forgets them all when it reaches
that limit.

## Specification compliance

oscpm implements the [OSC 1.0
specification](https://opensoundcontrol.stanford.edu/spec-1_0.html) with the
`//` operator OSC 1.1 took from XPath, which matches zero or more whole parts.

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
- `AddressSpace::dispatch` allocates nothing for a message to a registered
  address, and nothing for a pattern of up to `kMaxPatternLength` bytes it
  has dispatched before.
  The tests assert both.

Time:

- A pattern longer than `kMaxPatternLength` (1024 bytes) that contains `*`,
  `?`, `[`, `{` or `//` is not valid and is rejected before it is compiled; a
  literal pattern has no limit. The regex engine backtracks, so this bounds
  matching time without making it small: dispatching a 1024-byte pattern
  into 1,856 methods can take close to a second.


An `AddressSpace` is not safe to use from several threads at once.

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
