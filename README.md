# oscpm-regex

Header-only C++20 OpenSoundControl (OSC) address pattern matching built on
`std::regex`, in the most direct way possible.

oscpm-regex rewrites an OSC address pattern character by character into an
ECMAScript regular expression, `?` to `[^/]`, `*` to `[^/]*`, `{a,b}` to
`(?:a|b)`, `[!` to `[^`, an empty part to `(?:/[^/]*)*`, and hands
everything else to the regex engine: character classes, ranges, and the
decision of what is malformed. A `Matcher` memoises the verdict for every
pattern and address pair it has seen, and an `AddressSpace` dispatches by
asking the matcher about each of its methods. It depends on nothing outside
the standard library.

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
pattern.valid();                 // false if the regex engine rejected it
pattern.matches("/synth/2/amp"); // true

oscpm_regex::Matcher matcher;
matcher.match("/synth/*/freq", "/synth/1/freq"); // compiles, matches, remembers
matcher.match("/synth/*/freq", "/synth/1/freq"); // a hash lookup
```

`match` compiles the pattern and tests it once. A `Pattern` compiles once
and is matched many times; a pattern the regex engine rejects is invalid
and matches nothing. A `Matcher` memoises the verdict of every pattern and
address pair: the first call for a pair compiles and matches, every later
call for the same pair is a hash lookup. The memo grows without bound
until `clear`.

## Address space

```cpp
oscpm_regex::AddressSpace<Handler> methods;
methods.add("/synth/1/freq", setFrequency); // false if already registered
methods.remove("/synth/1/freq");            // false if not registered

const std::size_t matched = methods.dispatch(message.address(), [&](std::string_view address, Handler& handler)
    { handler(message); });
```

`dispatch` asks the address space's `Matcher` about every method in
insertion order and returns how many it visited, so a message costs one
memoised match per registered method. `matcher()` exposes the memo.

## What the regex engine decides

Because the translation carries no OSC rules of its own, the engine's
reading stands wherever the OSC 1.0 specification is silent, and it differs
from a hand-written matcher in these ways, found by running both corpora
of the oscpm comparison through it:

- A pattern without a leading `/` is not rejected; `a` matches `/a`.
- A space, `#` or non-ASCII byte in a pattern is a literal, so `/a b`
  matches the address `/a b`.
- A negated class matches `/`: `/[!x]` matches `//`.
- `?` and `*` inside a class are wildcards, so `[*]` and `[?]` become
  broken expressions and the pattern is invalid.
- A `]`, `}` or `,` outside its construct makes the pattern invalid or
  changes its meaning, where a hand-written matcher treats it as a literal:
  `/a,b` matches `/a` and `/b`.
- A wildcard or class inside braces works: `{a*,b}` matches `ax`.
- Braces nest: `{a,{b,c}}` matches `a`, `b` or `c`.
- A reversed range such as `[z-a]` compiles and matches nothing.

On the two corpora that is 40 of 472 and 37 of 453 cases; on random
patterns dense with brackets, braces and commas it is a third of them.

## Guarantees and their limits

- A memoised verdict allocates nothing, and so does a dispatch whose every
  pattern and address pair is memoised, which the tests assert.
- The first sight of a pattern and address pair compiles a `std::regex`
  and calls `std::regex_match`; both allocate. A new pattern against a
  thousand methods compiles a thousand times.
- Matching time is bounded by the regex engine; libc++'s does not
  backtrack catastrophically. The engine may throw `error_complexity` or
  `error_stack` on extreme inputs, which is not caught.
- Measured over 1,000 registered methods on an Apple Silicon Mac: a
  dispatch whose pairs are all memoised costs about 35 microseconds
  whatever the pattern, one hash lookup per method; the first dispatch of
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

`debug` and `release` use Ninja. The tests use Catch2, fetched at
configure time. The build fails on a clang-format violation when
oscpm-regex is the top-level project.

## Licence

zlib. See `LICENSE`.
