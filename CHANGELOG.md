# Changelog

## Unreleased

### Breaking changes

- `Pattern::parse` rejects a null or a byte above 0x7F as `PatternError::IllegalByte`, since an OSC-string holds only ASCII. Such patterns previously parsed: a part holding the byte matched no address, and a brace list still matched through its other members.

### Added

- `Address` and `Pattern` construct directly from text: `oscpm::Address("/synth/1/freq")`, `oscpm::Pattern("/synth/*")`. The constructor calls `std::abort` when the text is malformed, so a malformed literal in a constant expression fails to compile. `Address::parse` and `Pattern::parse` remain the calls for text whose fault the caller wants reported.

## 0.9.8 - 2026-10-13

### Breaking changes

- The `AddressSpace` calls are renamed after what they do: `invoke(pattern, args...)` is `dispatch(pattern, args...)`, `dispatch(pattern, visitor)` is `visit(pattern, visitor)`, `forEach(visitor)` is `visit(visitor)`, and `DispatchResult` is `MatchResult`.
- `Pattern::parse` returns `Expected<Pattern, PatternError>` and `Address::parse` returns `Expected<Address, AddressError>`. `Pattern::ParseResult` and `Address::ParseResult` are removed, and `Pattern` and `Address` have no default constructor.
- `Error` is replaced by `PatternError` (`MissingLeadingSlash`, `UnterminatedClass`, `UnterminatedBraces`, `PatternTooLong`) and `AddressError` (`MissingLeadingSlash`, `TrailingSlash`, `EmptyPart`, `IllegalByte`, `PartTooLong`), each with a `toString` overload. `MatchResult::error` is a `std::optional<PatternError>`.
- `Pattern::matches` takes an `Address`, and `match` parses its address argument as it parses its pattern, returning false for a malformed pattern, a malformed address or a non-match. A pattern ending in `/` matches no address.
- Releases are the tags on `main`, which is fast-forwarded to `develop` when a release is made. Pushes to `develop` are no longer tagged.

### Added

- `oscpm/expected.h`: `Expected<T, E>`, holding either a value or a fault, with the member names of `std::expected`: `has_value`, `operator bool`, `operator*`, `operator->` and `error`. Reading the side it does not hold calls `std::abort`.
- `AddressSpace::dispatch` calls each value through `std::invoke`, so a space of pointers to member functions takes the object, or a pointer to it, as the first of `args`.
- `AddressSpace<T>` asserts at compile time that `T` is an object type that is move-constructible and move-assignable, and `visit` and `dispatch` assert the visitor or handler signature, each with a message naming the requirement at the caller's own line.
- The headers compile with libstdc++ 7, as shipped by the GCC 7 Arduino cores. A compile-time call on that toolchain passes a `std::string_view` built with an explicit length, because the library's constant expressions cannot measure a literal there. Defining `OSCPM_STRING_VIEW_FALLBACK` selects the same code on any toolchain. CI compiles a sketch for the Arduino UNO R4 Minima, the ESP8266 and the Raspberry Pi Pico, and the README lists the boards checked.
- `CONTRIBUTING.md`, with the branch model, the changelog entry a pull request owes, and how a release is made.

### Changed

- `match` validates the address with a lookup table over the byte values. On x86-64 GCC 13 the `Match/parse` benchmarks execute 15% to 33% fewer instructions; `Pattern::matches` and the address space are unchanged.
- A space without a memo no longer carries a results array, and a space with one no longer zero-fills it on every visit. The peak stack of a wildcard visit on `AddressSpace<T, false>` falls from about 6.2 KiB to about 2.2 KiB. The README states the stack a visit or dispatch uses.

### Fixed

- A failed `Address::parse` no longer yields a readable empty `Address`, which `AddressSpace::add` read through a null pointer in a release build.
- `Expected`'s accessors contain no `assert`, so `oscpm/expected.h` compiles on toolchains whose `assert` expands to a static definition, such as the ESP8266 Arduino core, in a build without `NDEBUG`.

## 0.6.1 - 2026-10-04

### Breaking changes

- `AddressSpace::add` and `remove` take an `Address` and return whether the set changed: `add` returns false when the address is already registered, `remove` false when it is not. The overloads taking the address as bytes are removed, and with them `Error::Duplicate` and `Error::NotFound`.
- `ParseResult` is `Pattern::ParseResult`.

### Added

- `Address`, a validated address, in `oscpm/address.h`: `Address::parse` returns an `Address::ParseResult` holding the address or the first fault, at compile time for a fixed address, and `text` is the bytes it was parsed from. An address space registers methods under it, so an application that keeps addresses of its own validates each one where it enters, and `add` cannot fail on the address. A build without `NDEBUG` asserts in `add` and `remove` when the bytes an `Address` views have changed so that they no longer parse.

### Changed

- `oscpm/oscpm.h` includes `oscpm/address.h`.

## 0.5.6 - 2026-10-04

### Breaking changes

- `ParseError` is removed and no fault carries a byte offset. `ParseResult::error` returns the `Error`, and `DispatchResult::error` is a `std::optional<Error>`.
- `validatePattern` and `validateAddress` are no longer public. `Pattern::parse` reports a pattern's fault and `AddressSpace::add` an address's.
- `AddressSpace::lookup` is removed. `dispatch` is the one call that visits the methods a pattern matches; a caller holding a parsed `Pattern` passes `pattern.text()`.

### Added

- `AddressSpace::find`: a pointer to the value registered under an address, or null when there is none. The pointer is valid until the next `add` or `remove`.
- `AddressSpace::invoke(pattern, args...)`: calls `value(args...)` on every method the pattern matches, for a space whose values are handlers. It is `dispatch` with a visitor that passes `args` through, and every call receives the same `args` objects.
- `oscpm/pattern.h`, holding `Pattern`, `ParseResult` and `match`, and `oscpm/error.h`, holding `Error`, `toString`, `kMaxAddressPartLength` and `kMaxPatternLength`. Including `oscpm/pattern.h` gives the matcher without `AddressSpace` or the standard containers it uses.
- A build without `NDEBUG` asserts when a visitor adds or removes a method on the `AddressSpace` that called it, assigns to it or moves from it. A copy of a space made during a visit starts outside any visit. A release build is unchanged.

### Changed

- `oscpm/oscpm.h` includes `oscpm/error.h`, `oscpm/pattern.h` and `oscpm/address_space.h`.

### Fixed

- oscpm configures as a subproject of a project that has a CMake module named `ProjectVersion` or `CheckFormat` on its module path. oscpm included its own modules by name after appending its `cmake` directory to the module path, so the consumer's module was found first and oscpm configured with no version.
- `AddressSpace::dispatch` delivers the full result when a visitor dispatches another pattern on the same space. A nested call that used the memo entry being delivered could overwrite it, so the outer call visited the wrong methods or stopped early.

## 0.3.0 - 2026-09-23

### Breaking changes

- A pattern longer than 1024 bytes that contains `*`, `?`, `[`, `{` or `//` is malformed: `validatePattern` and `Pattern::parse` report `PatternTooLong` at offset 1024, and `match` and `AddressSpace::dispatch` visit nothing for it. A fault before offset 1024 is still reported first. A literal pattern has no limit.
- `Error::PatternTooLong` follows `Error::UnterminatedBraces`, so the enumerators after it have moved by one.

### Added

- `kMaxPatternLength`, the longest pattern containing a wildcard or `//` that `validatePattern` accepts.

## 0.2.9 - 2026-09-23

### Changed

- `AddressSpace` memoises a lookup of up to 1024 methods by default, up from 64, and stores each as a 4-byte index. The default memo takes about 1.1 MiB, allocated when the space is constructed. A space of 2^32 methods or more is not memoised.

## 0.2.7 - 2026-09-21

### Added

- `AddressSpace::dispatch` and `DispatchResult`: `Pattern::parse` followed by `lookup` in one call, taking the pattern as bytes and returning the number of methods visited together with the parse fault when there is one.

## 0.2.6 - 2026-09-21

### Breaking changes

- `isValidAddress` and `isValidPattern` are removed. `validateAddress` and `validatePattern` replace them, returning `std::optional<ParseError>`: the first fault by byte offset as an `Error` and its offset, or nothing when the input is well-formed.
- `match` rejects a malformed pattern before matching. A pattern without a leading `/` never matches, and a `[` or `{` left unclosed within its part never matches; every other pattern parses. An address without a leading `/` never matches.
- A trailing run of two or more slashes is the `//` operator, so `/a//` matches `/a` and every descendant of it. A single trailing `/` is still an empty part.
- A `{` inside a brace list is a literal byte and the first `}` closes the list: `{a,{b,c}}` is the members `a`, `{b` and `c` followed by a literal `}`. Such a pattern used to match nothing.
- A byte outside printable ASCII, a `#` or a space in a pattern is a literal that no address can contain. `isValidPattern` used to reject it; `match` compared it as it does now.

### Added

- `Error`, `ParseError` and `toString`.
- `Pattern`, a validated pattern parsed once and matched many times: `Pattern::parse` returns a `ParseResult` holding the pattern or its `ParseError`; `matches`, `text` and `isLiteral`. `match` is `parse` followed by `matches`.
- A literal pattern is matched by a single comparison, and a pattern part with no `*`, `?`, `[` or `{` by `==`.
- `AddressSpace` in `oscpm/address_space.h`: methods in bytewise address order, `add` and `remove` reporting `Duplicate` and `NotFound`, `lookup` fanning a pattern out to every matching method with a memo of recent wildcard lookups, and `forEach`.
- `corpus/matching.txt`, the conformance corpus, replayed by the test suite through `match`, `Pattern::matches`, both validators and the address space.
- A libFuzzer target behind `OSCPM_BUILD_FUZZERS`, seeded from the corpus, run under AddressSanitizer and UndefinedBehaviorSanitizer in CI; `OSCPM_SANITIZE` builds the tests under the same sanitizers.
- `examples/dispatch.cpp` behind `OSCPM_BUILD_EXAMPLES`, dispatching an oscpp bundle through an address space.
- The installed package config accepts the same minor version while the major version is 0.

## 0.1.0 - 2026-09-19

- `match`, `isValidAddress`, `isValidPattern` and `kMaxAddressPartLength`.
