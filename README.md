# oscpm

OSC address pattern matching for C++17. A header-only complement to
[oscpp](https://github.com/kaoskorobase/oscpp), which encodes and decodes OSC
packets but leaves pattern matching to you.

## Pattern syntax

OSC 1.0 pattern matching plus the OSC 1.1 descendant operator:

| Syntax      | Matches                                                      |
| ----------- | ------------------------------------------------------------ |
| `?`         | any single character except `/`                              |
| `*`         | any run of zero or more characters, none of them `/`         |
| `[abc]`     | any one of the listed characters                             |
| `[a-z]`     | any character in the ASCII range                             |
| `[!abc]`    | any character not listed                                     |
| `{foo,bar}` | any one of the comma-separated literal alternatives          |
| `//`        | zero or more whole address parts                             |

Every other printable ASCII character matches itself. A pattern and an
address must both begin with `/`.

Alternatives inside `{}` are literal text, so `{a*,b}` is rejected. An empty
alternative is allowed and makes the group optional: `/x{a,}` matches `/x` and
`/xa`.

## Usage

```cpp
#include <oscpm/oscpm.hpp>

// One-off match. Validates the pattern first; malformed patterns never match.
bool hit = oscpm::match("/synth/[0-9]/{freq,amp}", "/synth/3/amp");

// Validate once, match many times.
oscpm::Pattern p("/mixer/ch?/gain*");
if (!p) {
    std::fprintf(stderr, "bad pattern: %s\n", oscpm::errorMessage(p.error()));
}
bool hit2 = p.matches("/mixer/ch1/gain_db");

// Ask why a pattern is malformed.
oscpm::Error e = oscpm::validate("/synth/[0-9");  // Error::UnterminatedCharacterClass
```

With oscpp, pass the address straight through:

```cpp
OSCPP::Server::Message msg = ...;
if (p.matches(msg.address())) { ... }
```

`match`, `validate` and everything they call are `constexpr`, so a fixed
pattern can be checked at compile time with `static_assert`.

## Guarantees

- No heap allocation and no exceptions in the matching path. `Pattern`
  construction copies the pattern text and may allocate; `Pattern::matches`,
  `match` and `validate` never do.
- Malformed patterns are rejected, not reinterpreted. `validate` says why.
- No dependencies beyond the standard library.

Matching backtracks at `*`, `{}` and `//`. OSC patterns are short in practice,
but a pattern with many `*` in one part can take time quadratic or worse in
the part length.

## Building and testing

The library is a single header; copy `include/oscpm/oscpm.hpp` or use CMake:

```cmake
add_subdirectory(oscpm)
target_link_libraries(myapp PRIVATE oscpm::oscpm)
```

To run the tests (Catch2 is found via `find_package` or fetched):

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## License

zlib. See [LICENSE](LICENSE).
