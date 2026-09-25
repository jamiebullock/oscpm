# Changelog

## Unreleased

### Added

- `oscpm/pattern.h`, holding `Pattern`, `ParseResult`, `match`, `validatePattern` and `validateAddress`, and `oscpm/error.h`, holding `Error`, `ParseError`, `toString`, `kMaxAddressPartLength` and `kMaxPatternLength`. Including `oscpm/pattern.h` gives the matcher without `AddressSpace` or the standard containers it uses.

### Changed

- `oscpm/oscpm.h` includes `oscpm/error.h`, `oscpm/pattern.h` and `oscpm/address_space.h`.

### Fixed

- oscpm configures as a subproject of a project that has a CMake module named `ProjectVersion` or `CheckFormat` on its module path. oscpm included its own modules by name after appending its `cmake` directory to the module path, so the consumer's module was found first and oscpm configured with no version.
- `AddressSpace::dispatch` and `lookup` deliver the full result when a visitor dispatches or looks up another pattern on the same space. A nested call that used the memo entry being delivered could overwrite it, so the outer call visited the wrong methods or stopped early.

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
