# Changelog

## [0.10.1] - 2026-10-17

### Changed

- Faster `Pattern::parse`, `Address::parse` and `match` (#77)

## [0.10.0] - 2026-10-16

### Breaking changes

- `Pattern::parse` rejects a null or a byte above 0x7F as `PatternError::IllegalByte` (#69)

### Added

- `Address` and `Pattern` constructors taking text, which call `std::abort` on malformed text (#65)

### Changed

- Faster `Pattern::matches`, `match` and `Pattern::parse` (#73)

## [0.9.8] - 2026-10-13

### Breaking changes

- `AddressSpace`: `invoke` is renamed `dispatch`, `dispatch` and `forEach` are renamed `visit`, and `DispatchResult` is renamed `MatchResult` (#49)
- `Pattern::parse` and `Address::parse` return `Expected`; `ParseResult` and the default constructors of `Pattern` and `Address` are removed (#51)
- `Error` is replaced by `PatternError` and `AddressError`, and `MatchResult::error` is a `std::optional<PatternError>` (#51)
- `Pattern::matches` takes an `Address`, and `match` returns false for a malformed address (#53)
- A pattern ending in a single `/` matches no address (#53)
- Releases are the tags on `main`, and pushes to `develop` are no longer tagged (#61)

### Added

- `Expected<T, E>` in `oscpm/expected.h` (#51)
- `AddressSpace::dispatch` accepts pointers to member functions as values (#48)
- Compile-time checks of the `AddressSpace` value type and of visitor and handler signatures (#47, #50)
- The headers compile with libstdc++ 7, as shipped by the GCC 7 Arduino cores (#54)

### Changed

- Faster address validation in `match` (#59)
- `AddressSpace<T, false>` uses less stack in a wildcard visit (#46)

### Fixed

- `AddressSpace::add`: null pointer read when passed the `Address` from a failed `Address::parse` (#51)
- `oscpm/expected.h` compiles where `assert` expands to a static definition, such as the ESP8266 Arduino core (#55)

## [0.6.1] - 2026-10-04

### Breaking changes

- `AddressSpace::add` and `remove` take an `Address` and return whether the set changed (#44)
- `Error::Duplicate` and `Error::NotFound` are removed (#44)
- `ParseResult` is renamed `Pattern::ParseResult` (#44)

### Added

- `Address` and `Address::parse` in `oscpm/address.h` (#44)
- Debug-build assertion in `AddressSpace::add` and `remove` when the bytes an `Address` views no longer parse (#44)

### Changed

- `oscpm/oscpm.h` includes `oscpm/address.h` (#44)

## [0.5.6] - 2026-10-04

### Breaking changes

- `ParseError` is removed, and no fault carries a byte offset (#33)
- `ParseResult::error` returns an `Error`, and `DispatchResult::error` is a `std::optional<Error>` (#33)
- `validatePattern` and `validateAddress` are no longer public (#33)
- `AddressSpace::lookup` is removed; call `dispatch` with `pattern.text()` (#37)

### Added

- `AddressSpace::find` (#37)
- `AddressSpace::invoke` (#40)
- `oscpm/pattern.h` and `oscpm/error.h` (#26)
- Debug-build assertion when a visitor modifies the `AddressSpace` that called it (#32)

### Changed

- `oscpm/oscpm.h` includes `oscpm/error.h`, `oscpm/pattern.h` and `oscpm/address_space.h` (#26)

### Fixed

- Configuring as a subproject of a project that has a `ProjectVersion` or `CheckFormat` CMake module (#31)
- `AddressSpace::dispatch`: wrong or missing visits when a visitor dispatched on the same space (#21)

## [0.3.0] - 2026-09-23

### Breaking changes

- A pattern over 1024 bytes that contains `*`, `?`, `[`, `{` or `//` is rejected as `PatternTooLong` (#15)
- `Error::PatternTooLong` follows `Error::UnterminatedBraces`, moving the later enumerators by one (#15)

### Added

- `kMaxPatternLength` (#15)

## [0.2.10] - 2026-09-23

### Changed

- `AddressSpace` memoises a lookup of up to 1024 methods by default, up from 64 (#14)
- The default `AddressSpace` memo takes about 1.1 MiB, allocated at construction (#14)

## [0.2.7] - 2026-09-21

### Added

- `AddressSpace::dispatch` and `DispatchResult` (#11)

## [0.2.6] - 2026-09-21

### Breaking changes

- `isValidAddress` and `isValidPattern` are replaced by `validateAddress` and `validatePattern`, which return `std::optional<ParseError>` (#4)
- `match` returns false for a pattern or an address without a leading `/` (#4)
- `match` returns false for a pattern with a `[` or `{` left unclosed within its part (#4)
- A trailing run of two or more slashes is the `//` operator, so `/a//` matches `/a` and its descendants (#4)
- A `{` inside a brace list is a literal byte, and the first `}` closes the list (#4)
- `validatePattern` accepts a byte outside printable ASCII, a `#` or a space, which `isValidPattern` rejected (#4)

### Added

- `Error`, `ParseError` and `toString` (#4)
- `Pattern` and `ParseResult`, with `Pattern::parse`, `matches`, `text` and `isLiteral` (#5)
- `AddressSpace` in `oscpm/address_space.h`, with `add`, `remove`, `lookup` and `forEach` (#8)
- `corpus/matching.txt`, the conformance corpus (#4)
- `OSCPM_BUILD_FUZZERS` and `OSCPM_SANITIZE` build options (#7, #8)
- `examples/dispatch.cpp`, built with `OSCPM_BUILD_EXAMPLES` (#9)

### Changed

- Faster matching of literal patterns and literal parts (#6)
- `find_package(oscpm)` accepts only the same minor version while the major version is 0 (#3)

## [0.1.0] - 2026-09-19

### Added

- `match`, `isValidAddress`, `isValidPattern` and `kMaxAddressPartLength` (#2)

[0.10.1]: https://github.com/jamiebullock/oscpm/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/jamiebullock/oscpm/compare/v0.9.8...v0.10.0
[0.9.8]: https://github.com/jamiebullock/oscpm/compare/v0.6.1...v0.9.8
[0.6.1]: https://github.com/jamiebullock/oscpm/compare/v0.5.6...v0.6.1
[0.5.6]: https://github.com/jamiebullock/oscpm/compare/v0.3.0...v0.5.6
[0.3.0]: https://github.com/jamiebullock/oscpm/compare/v0.2.10...v0.3.0
[0.2.10]: https://github.com/jamiebullock/oscpm/compare/v0.2.7...v0.2.10
[0.2.7]: https://github.com/jamiebullock/oscpm/compare/v0.2.6...v0.2.7
[0.2.6]: https://github.com/jamiebullock/oscpm/compare/v0.1.0...v0.2.6
[0.1.0]: https://github.com/jamiebullock/oscpm/compare/v0.0.1...v0.1.0
