# Changelog

## [0.10.1] - 2026-10-17

### Changed

- Speed up `Pattern::parse`, `Address::parse` and `match` ([#77](https://github.com/jamiebullock/oscpm/pull/77))

## [0.10.0] - 2026-10-16

### Changed

- **Breaking:** reject a null or a byte above 0x7F in `Pattern::parse` ([#69](https://github.com/jamiebullock/oscpm/pull/69))
- Speed up `Pattern::matches`, `match` and `Pattern::parse` ([#73](https://github.com/jamiebullock/oscpm/pull/73))

### Added

- Add `Address` and `Pattern` constructors taking text ([#65](https://github.com/jamiebullock/oscpm/pull/65))

## [0.9.8] - 2026-10-13

### Changed

- **Breaking:** rename `AddressSpace::invoke` to `dispatch`, `dispatch` and `forEach` to `visit`, and `DispatchResult` to `MatchResult` ([#49](https://github.com/jamiebullock/oscpm/pull/49))
- **Breaking:** return `Expected` from `Pattern::parse` and `Address::parse` ([#51](https://github.com/jamiebullock/oscpm/pull/51))
- **Breaking:** replace `Error` with `PatternError` and `AddressError`, and make `MatchResult::error` a `std::optional<PatternError>` ([#51](https://github.com/jamiebullock/oscpm/pull/51))
- **Breaking:** take an `Address` in `Pattern::matches`, and return false from `match` for a malformed address ([#53](https://github.com/jamiebullock/oscpm/pull/53))
- **Breaking:** treat a pattern ending in a single `/` as matching no address ([#53](https://github.com/jamiebullock/oscpm/pull/53))
- **Breaking:** tag releases on `main`, and stop tagging pushes to `develop` ([#61](https://github.com/jamiebullock/oscpm/pull/61))
- Speed up address validation in `match` ([#59](https://github.com/jamiebullock/oscpm/pull/59))
- Reduce the stack `AddressSpace<T, false>` uses in a wildcard visit ([#46](https://github.com/jamiebullock/oscpm/pull/46))

### Added

- Add `Expected<T, E>` in `oscpm/expected.h` ([#51](https://github.com/jamiebullock/oscpm/pull/51))
- Support pointers to member functions as values in `AddressSpace::dispatch` ([#48](https://github.com/jamiebullock/oscpm/pull/48))
- Add compile-time checks of the `AddressSpace` value type and of visitor and handler signatures ([#47](https://github.com/jamiebullock/oscpm/pull/47), [#50](https://github.com/jamiebullock/oscpm/pull/50))
- Support libstdc++ 7, as supplied with the GCC 7 Arduino cores ([#54](https://github.com/jamiebullock/oscpm/pull/54))

### Removed

- **Breaking:** remove `ParseResult` and the default constructors of `Pattern` and `Address` ([#51](https://github.com/jamiebullock/oscpm/pull/51))

### Fixed

- Fix a null pointer read in `AddressSpace::add` when passed the `Address` from a failed `Address::parse` ([#51](https://github.com/jamiebullock/oscpm/pull/51))
- Fix compiling `oscpm/expected.h` where `assert` expands to a static definition, such as the ESP8266 Arduino core ([#55](https://github.com/jamiebullock/oscpm/pull/55))

## [0.6.1] - 2026-10-04

### Changed

- **Breaking:** take an `Address` in `AddressSpace::add` and `remove`, and return whether the set changed ([#44](https://github.com/jamiebullock/oscpm/pull/44))
- **Breaking:** rename `ParseResult` to `Pattern::ParseResult` ([#44](https://github.com/jamiebullock/oscpm/pull/44))
- Include `oscpm/address.h` from `oscpm/oscpm.h` ([#44](https://github.com/jamiebullock/oscpm/pull/44))

### Added

- Add `Address` and `Address::parse` in `oscpm/address.h` ([#44](https://github.com/jamiebullock/oscpm/pull/44))
- Add a debug-build assertion in `AddressSpace::add` and `remove` when the bytes an `Address` views no longer parse ([#44](https://github.com/jamiebullock/oscpm/pull/44))

### Removed

- **Breaking:** remove `Error::Duplicate` and `Error::NotFound` ([#44](https://github.com/jamiebullock/oscpm/pull/44))

## [0.5.6] - 2026-10-04

### Changed

- **Breaking:** return an `Error` from `ParseResult::error`, and make `DispatchResult::error` a `std::optional<Error>` ([#33](https://github.com/jamiebullock/oscpm/pull/33))
- Include `oscpm/error.h`, `oscpm/pattern.h` and `oscpm/address_space.h` from `oscpm/oscpm.h` ([#26](https://github.com/jamiebullock/oscpm/pull/26))

### Added

- Add `AddressSpace::find` ([#37](https://github.com/jamiebullock/oscpm/pull/37))
- Add `AddressSpace::invoke` ([#40](https://github.com/jamiebullock/oscpm/pull/40))
- Add `oscpm/pattern.h` and `oscpm/error.h` ([#26](https://github.com/jamiebullock/oscpm/pull/26))
- Add a debug-build assertion when a visitor modifies the `AddressSpace` that called it ([#32](https://github.com/jamiebullock/oscpm/pull/32))

### Removed

- **Breaking:** remove `ParseError` and the byte offset it carried ([#33](https://github.com/jamiebullock/oscpm/pull/33))
- **Breaking:** remove `validatePattern` and `validateAddress` from the public API ([#33](https://github.com/jamiebullock/oscpm/pull/33))
- **Breaking:** remove `AddressSpace::lookup` in favour of `dispatch` with `pattern.text()` ([#37](https://github.com/jamiebullock/oscpm/pull/37))

### Fixed

- Fix configuring as a subproject of a project that has a `ProjectVersion` or `CheckFormat` CMake module ([#31](https://github.com/jamiebullock/oscpm/pull/31))
- Fix wrong or missing visits in `AddressSpace::dispatch` when a visitor dispatched on the same space ([#21](https://github.com/jamiebullock/oscpm/pull/21))

## [0.3.0] - 2026-09-23

### Changed

- **Breaking:** reject a pattern over 1024 bytes that contains `*`, `?`, `[`, `{` or `//` ([#15](https://github.com/jamiebullock/oscpm/pull/15))

### Added

- **Breaking:** add `Error::PatternTooLong` after `Error::UnterminatedBraces`, moving the later enumerators by one ([#15](https://github.com/jamiebullock/oscpm/pull/15))
- Add `kMaxPatternLength` ([#15](https://github.com/jamiebullock/oscpm/pull/15))

## [0.2.10] - 2026-09-23

### Changed

- Raise the default `AddressSpace` memo result limit from 64 to 1024 methods, which takes more memory ([#14](https://github.com/jamiebullock/oscpm/pull/14))

## [0.2.7] - 2026-09-21

### Added

- Add `AddressSpace::dispatch` and `DispatchResult` ([#11](https://github.com/jamiebullock/oscpm/pull/11))

## [0.2.6] - 2026-09-21

### Changed

- **Breaking:** replace `isValidAddress` and `isValidPattern` with `validateAddress` and `validatePattern`, which return `std::optional<ParseError>` ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** return false from `match` for a pattern or an address without a leading `/` ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** return false from `match` for a pattern with a `[` or `{` left unclosed within its part ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** treat two or more trailing slashes as the `//` operator, as in `/a//` matching `/a` and its descendants ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** treat a `{` inside a brace list as a literal byte, and close the list at the first `}` ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** accept a byte outside printable ASCII, a `#` or a space in `validatePattern`, which `isValidPattern` rejected ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- **Breaking:** accept only the same minor version in `find_package(oscpm)` while the major version is 0 ([#3](https://github.com/jamiebullock/oscpm/pull/3))
- Speed up matching of literal patterns and literal parts ([#6](https://github.com/jamiebullock/oscpm/pull/6))

### Added

- Add `Error`, `ParseError` and `toString` ([#4](https://github.com/jamiebullock/oscpm/pull/4))
- Add `Pattern` and `ParseResult`, with `Pattern::parse`, `matches`, `text` and `isLiteral` ([#5](https://github.com/jamiebullock/oscpm/pull/5))
- Add `AddressSpace` in `oscpm/address_space.h`, with `add`, `remove`, `lookup` and `forEach` ([#8](https://github.com/jamiebullock/oscpm/pull/8))
- Add `examples/dispatch.cpp`, built with `OSCPM_BUILD_EXAMPLES` ([#9](https://github.com/jamiebullock/oscpm/pull/9))

## [0.1.0] - 2026-09-19

### Added

- Add `match`, `isValidAddress`, `isValidPattern` and `kMaxAddressPartLength` ([#2](https://github.com/jamiebullock/oscpm/pull/2))

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
