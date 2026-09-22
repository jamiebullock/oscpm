# oscpm

oscpm is a header-only [Open Sound Contro](https://opensoundcontrol.stanford.edu/spec-1_0.html)
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
pattern.valid();                 // false if the regex engine rejected it
pattern.matches("/synth/2/amp"); // true

oscpm::Matcher matcher;
matcher.match("/synth/*/freq", "/synth/1/freq"); // compiles, matches, remembers
matcher.match("/synth/*/freq", "/synth/1/freq"); // a hash lookup
```

`match` compiles the pattern and tests it once. A `Pattern` compiles once
and is matched many times; a pattern the regex engine rejects is invalid
and matches nothing. A `Matcher` memoises the verdict of every pattern and
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
pattern is put to the address space's `Matcher` for every method, in no
particular order, so a wildcard message costs one memoised match per
registered method.

## What the regex engine decides

Because the translation carries no OSC rules of its own, the engine's
reading stands wherever the OSC 1.0 specification is silent:

- A pattern without a leading `/` is not rejected; `a` matches `/a`.
- A space, `#` or non-ASCII byte in a pattern is a literal, so `/a b`
  matches the address `/a b`.
- A negated class matches `/`: `/[!x]` matches `//`.
- `?` and `*` inside a class are wildcards, so `[*]` and `[?]` become
  broken expressions and the pattern is invalid.
- A `]`, `}` or `,` outside its construct makes the pattern invalid or
  changes its meaning: `/a,b` matches `/a` and `/b`.
- A wildcard or class inside braces works: `{a*,b}` matches `ax`.
- Braces nest: `{a,{b,c}}` matches `a`, `b` or `c`.

A reversed range such as `[z-a]` matches nothing, as in glob; libc++
compiles it as an empty class, and a standard library that rejects it
instead makes the pattern invalid, which also matches nothing.

## Guarantees and their limits

- A memoised verdict allocates nothing, and so does a dispatch whose every
  pattern and address pair is memoised, which the tests assert.
- The first sight of a pattern and address pair compiles a `std::regex`
  and calls `std::regex_match`; both allocate. A new pattern against a
  thousand methods compiles a thousand times.
- Whether matching time is bounded, and whether a pair is decided at all, is
  a property of the standard library rather than of oscpm. `matches` catches
  every `regex_error`, so a pair the engine abandons reads as no match.
- libc++ abandons a pair once its step count passes 4,096 times the address
  length, in 5 to 25 milliseconds. The MSVC STL abandons one after ten
  million node visits or a thousand levels of recursion, in 1 millisecond to
  1.3 seconds. libstdc++ has no limit and decides every pair, given time.
- A pair that would have matched but exceeds a budget therefore reads as no
  match, and the three libraries can answer the same input differently.
  `/*a` repeated 20 times then `*b` against a part of 200 `a`s ending in `b`
  matches: libstdc++ returns true in 62 milliseconds, libc++ and the MSVC STL
  return false. `//a` repeated three times then `//b` against a 200-part
  address matches: libstdc++ returns true, the MSVC STL returns false after
  1.5 milliseconds.
- Where there is no budget there is no bound: on libstdc++ `/*a*a*b` against
  a 200-byte part takes 55 milliseconds, `/*a*a*a*b` 2.7 seconds,
  `/*a*a*a*a*b` 102 seconds, `/{a,aa}` repeated 32 times 116 seconds, and
  `/{a,}` repeated 50 times does not finish. A caller that takes patterns
  from an untrusted source needs its own limit on their length and operator
  count.
- A budget is per pair, so `AddressSpace::dispatch` pays it once per
  registered method. Measured over 1,000 methods whose addresses are 58 bytes,
  the pattern `/synth/*/*a*a*a*b` returns no match in 2.9 seconds on libc++,
  and 0.06 milliseconds on every later message once the memo holds those
  pairs. A sender that varies such patterns pays the first cost each time.
- Measured over 1,000 registered methods on an Apple Silicon Mac: a
  message to a registered address costs about 10 nanoseconds; a wildcard
  dispatch whose pairs are all memoised costs about 35 microseconds, one
  hash lookup per method; the first dispatch of
  a new pattern costs 1 to 4 milliseconds and tens of thousands of
  allocations. A single memoised `match` costs about 20 nanoseconds.
- A `Matcher` and an `AddressSpace` are not safe to use from several
  threads at once.

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
