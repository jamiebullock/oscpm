# oscpm

OSC address pattern matching for C++17. A header-only complement to
[oscpp](https://github.com/kaoskorobase/oscpp), which builds and parses OSC packets but
deliberately leaves "message address pattern matching and bundle scheduling to the caller".
oscpm covers the first half of that.

- Matches [OSC 1.0](https://opensoundcontrol.stanford.edu/spec-1_0.html) address patterns
  (`?`, `*`, `[...]`, `{...}`) plus the OSC 1.1 `//` operator.
- Validates patterns and addresses with error codes and byte offsets.
- `constexpr` matcher and validators, no allocation, no exceptions on the matching path.
- An `AddressSpace<T>` that maps literal addresses to values and dispatches a pattern to every
  match, with a fast path for literal patterns and prefix narrowing for the rest.
- An optional adapter that dispatches oscpp packets, recursing into bundles.

## Usage

```cpp
#include <oscpm/oscpm.hpp>

oscpm::match("/synth/*/freq", "/synth/1/freq");   // true
oscpm::match("/synth/*", "/synth/1/freq");        // false: '*' does not cross '/'
oscpm::match("//freq", "/synth/1/freq");          // true: OSC 1.1 "//"

// Validate a pattern once, match many times.
auto p = oscpm::Pattern::compile("/synth/[0-9]/{freq,amp}");
if (!p) {
    oscpm::Error e = oscpm::validate_pattern("...");
    // e.code, e.position, e.message()
}
p->matches("/synth/3/amp");                        // true

// Or throw on a bad pattern.
oscpm::Pattern q("/synth/[");                      // throws oscpm::PatternError
```

Dispatching to handlers:

```cpp
#include <oscpm/address_space.hpp>

oscpm::AddressSpace<std::function<void(float)>> space;
space.add("/synth/1/freq", [](float f) { /* ... */ });
space.add("/synth/2/freq", [](float f) { /* ... */ });

space.dispatch("/synth/*/freq", [](const std::string& address, auto& handler) {
    handler(440.f);
});
```

With oscpp, dispatch a whole packet, bundles included:

```cpp
#include <oscpm/oscpp.hpp>

OSCPP::Server::Packet packet(data, size);
oscpm::dispatch(space, packet, [](const OSCPP::Server::Message& m, const std::string& address, auto& handler) {
    handler(m.args().float32());
});
```

Bundle time tags are ignored. Scheduling stays with the caller, as in oscpp.

## Pattern semantics

Both sides must begin with `/`. A pattern is matched part by part against the address, where
parts are separated by `/`. Within a part:

| Syntax    | Matches                                                                          |
|-----------|----------------------------------------------------------------------------------|
| `?`       | any single character                                                             |
| `*`       | any run of zero or more characters                                               |
| `[abc]`   | one character from the set; `[a-z]` is an inclusive range                        |
| `[!abc]`  | one character not in the set                                                     |
| `{foo,bar}` | any one of the comma-separated literal strings; an empty alternative is allowed |
| other     | itself                                                                           |

None of these match `/`. A `-` first or last inside `[...]` is literal, as is `!` anywhere but
first. The strings inside `{...}` are literal; wildcards inside them are not interpreted.

`//` (OSC 1.1) matches zero or more whole parts, so `/a//c` matches `/a/c`, `/a/b/c` and
`/a/b/x/c`.

`validate_pattern` rejects: empty input, a missing leading `/`, a trailing `/`, three or more
consecutive slashes, unterminated or unmatched `[]` and `{}`, nested `{}`, a `/` inside `[]` or
`{}`, and reversed ranges such as `[z-a]`. `validate_address` additionally rejects `//` and any of
the reserved characters `space # * , ? [ ] { }`.

`match` itself never validates. Given a malformed pattern or address it returns something well
defined but unspecified, without undefined behaviour.

## Building and testing

```shell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Tests use Catch2, fetched at configure time. The oscpp integration tests and example are built
when `OSCPM_WITH_OSCPP` is on (the default), which fetches oscpp as well. Turn it off for a build
with no network access beyond Catch2.

## Adding to a project

```cmake
include(FetchContent)
FetchContent_Declare(oscpm
    GIT_REPOSITORY <url of this repository>
    GIT_TAG        main
)
FetchContent_MakeAvailable(oscpm)

target_link_libraries(myapp PRIVATE oscpm::oscpm)
```

Or copy the `include` directory onto your include path. Only `<oscpm/oscpp.hpp>` needs oscpp.

## License

Boost Software License 1.0, the same as oscpp.
