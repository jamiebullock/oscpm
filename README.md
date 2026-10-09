# oscpm

Header-only C++17 OpenSoundControl (OSC) address pattern matching.

[![CI](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml)
[![Latest tag](https://img.shields.io/github/v/tag/jamiebullock/oscpm?sort=semver&label=tag)](https://github.com/jamiebullock/oscpm/tags)
[![Licence](https://img.shields.io/github/license/jamiebullock/oscpm?label=licence)](LICENSE)

oscpm matches OSC address patterns against addresses, and through an address space visits or dispatches to the values registered under the matching addresses. It supports the [OSC 1.0](https://opensoundcontrol.stanford.edu/spec-1_0.html) matching syntax, and the proposed `//` operator from [OSC 1.1](https://opensoundcontrol.stanford.edu/spec-1_1.html).

It is designed to complement [oscpp](https://github.com/kaoskorobase/oscpp), which reads and writes OSC packets, leaving matching and dispatch to the caller.  Like oscpp, it suits realtime contexts, with matching and dispatch (`matches()`, `visit()` and `dispatch()`) allocation free with documented [memory and time guarantees](#guarantees). 

oscpm does not depend on oscpp, or on anything outside the standard library.

## Example

```cpp
#include <oscpm/address_space.h>
#include <oscpp/server.hpp>

#include <functional>

using Handler = std::function<void(const OSCPP::Server::Message&)>;

void setFrequency(const OSCPP::Server::Message& message)
{
    synth.setFrequency(OSCPP::Server::ArgStream(message.args()).float32());
}

oscpm::AddressSpace<Handler> methods;
methods.add(*oscpm::Address::parse("/synth/freq"), setFrequency);

// for each OSCPP::Server::Message received
methods.dispatch(message.address(), message); // A message with address "/synth/*" calls setFrequency()
```

## Matching
In the simple case a match can be checked by comparing two `std::string_view`:

```cpp
#include <oscpm/pattern.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true (OSC 1.1 syntax)
oscpm::match("/synth/[1-3]/{freq,amp}", "/synth/2/amp"); // true
oscpm::match("/synth/*/freq", "/1/synth/freq"); // false
```

> `oscpm::match()` returns `false` for both a pattern parse failure and a non-match. If the distinction matters then [Pattern::parse()](#patterns) should be used instead.

## Patterns
A pattern that will be matched many times can be validated once and kept as a `Pattern` object. This has the following advantages:
- A malformed pattern is reported as a `PatternError` so the caller can distinguish an invalid pattern from a non-match
- For `constexpr` patterns, validity can be checked at compile time
- Matching with `Pattern::matches()` is faster than `oscpm::match()` because validation happens once in `Pattern::parse()`, and patterns without wildcards reduce to a plain string comparison

```cpp
constexpr auto parsed = oscpm::Pattern::parse("/synth/*/{freq,amp}");
static_assert(parsed);
static_assert(parsed->matches("/synth/12/amp"));

if (const auto result = oscpm::Pattern::parse(text))
{
    result->matches(message.address()); // any std::string_view
}
else
{
    oscpm::toString(result.error()); // UnterminatedClass for "/synth/[1-3"
}
```

## Addresses
Similar to `Pattern`, `Address` is a validated OSC address:
- A malformed address is reported as an `AddressError`, one of the faults under [Malformed Addresses](#malformed-addresses)
- For `constexpr` addresses, validity can be checked at compile time

```cpp
#include <oscpm/address.h>
#include <oscpp/client.hpp>

constexpr auto parsed = oscpm::Address::parse("/synth/1/freq");
static_assert(parsed);

// Validate e.g. a user-typed address
if (const auto result = oscpm::Address::parse(userAddress))
{
    packet.openMessage(userAddress.c_str(), 1).float32(value).closeMessage();
}
else
{
    showError(oscpm::toString(result.error())); // IllegalByte for "/mixer/ch 1/gain"
}
```

## Address Space

`AddressSpace<T>` associates values of a caller-chosen type `T` with OSC addresses. Depending on the type chosen for `T`, an OSC method can correspond to a function the caller supplies or a parameter the address space itself manages.

### Registering Callbacks

In the common form, the caller registers a callback for each address. When a received OSC message is dispatched to the address space, every callback whose address matches the message's address pattern is called with the message as an argument. The mixer example below shows this:

```cpp
// A message received from the OSC client, e.g. { "/mixer/*/gain", 0.5f } or { "/mixer/3/mute", 1 }
struct Message
{
  std::string_view addressPattern;
  std::variant<float, std::int32_t, std::string_view> argument;
};

// In a real application this would read the next incoming message into message
bool receive(Message& message);

int main()
{
  std::array<float, 8> gains {};
  std::array<bool, 8> mutes {};
  oscpm::AddressSpace<std::function<void(const Message&)>> handlers;

  // Add a gain and a mute handler for each mixer channel
  for (std::size_t channel = 0; channel < gains.size(); ++channel)
  {
      const std::string prefix = "/mixer/" + std::to_string(channel + 1);
      handlers.add(*oscpm::Address::parse(prefix + "/gain"), [&gains, channel](const Message& message)
          { gains[channel] = std::get<float>(message.argument); });
      handlers.add(*oscpm::Address::parse(prefix + "/mute"), [&mutes, channel](const Message& message)
          { mutes[channel] = std::get<std::int32_t>(message.argument) != 0; });
  }

  // The main receive loop
  Message message;
  while (receive(message))
  {
      // Dispatch the OSC message to every method whose address matches the received pattern, invoking the corresponding handler on each match
      const oscpm::MatchResult result = handlers.dispatch(message.addressPattern, message);
      if (result.error)
      {
          std::fprintf(stderr, "malformed pattern %.*s: %s\n", static_cast<int>(message.addressPattern.size()), message.addressPattern.data(), oscpm::toString(*result.error));
      }
      else if (result.matched == 0)
      {
          std::fprintf(stderr, "no channel at %.*s\n", static_cast<int>(message.addressPattern.size()), message.addressPattern.data());
      }
  }

  // Print the new values set by the handlers
  for (std::size_t channel = 0; channel < gains.size(); ++channel)
  {
      std::printf("%zu %g%s\n", channel + 1, static_cast<double>(gains[channel]), mutes[channel] ? " muted" : "");
  }
  return 0;
}
```

### Registering Parameters

In the alternative form, the address space stores the parameters directly. This suits an application where every address holds the same kind of value and only the latest value matters, because the values are sampled when they are needed rather than acted on as each message arrives. A message that must have an effect each time it is received, such as a trigger, needs the callback form.

Using oscpm in this way, the caller registers a _value_ instead of a callback for each address and uses `visit()` in place of `dispatch()`. 

The visitor is invoked once for each method whose address matches the received pattern. Since a method corresponds to a value in this use case, the visitor's role is simply to assign the OSC message's argument to the stored value. Values can then be read back individually by address with `find()`, or all together with `visit(visitor)`. 
 
The example below is a lighting rig: every address is a dimmer level, a network thread queues the console's messages, and the output thread applies them and reads the current level of every dimmer each frame. The address space is only ever touched by the output thread:

```cpp
// A message received from the lighting console, e.g. { "/light/[01-04]/level", 0.75f } or { "/light/*/level", 0.0f }
struct LevelMessage
{
  char addressPattern[64];
  float level;
};

// A lock-free single-producer single-consumer queue: the network thread pushes each received message and the output thread pops them
template <typename T>
class SpscQueue
{
public:
  bool push(const T& message);
  bool pop(T& message);
};

SpscQueue<LevelMessage> received;

// Called from the output thread on each frame
void outputFrame(oscpm::AddressSpace<float>& levels)
{
  // For each message received since the last frame, set every level whose address matches its pattern
  LevelMessage message;
  while (received.pop(message))
  {
      levels.visit(message.addressPattern, [&message](std::string_view, float& level)
          { level = message.level; });
  }

  // Read one level back by its address
  std::printf("dimmer 3: %g\n", static_cast<double>(*levels.find("/light/03/level")));

  // Or read every level in address order to build the frame
  levels.visit([](std::string_view address, float level)
      { std::printf("%.*s %g\n", static_cast<int>(address.size()), address.data(), static_cast<double>(level)); });
}

int main()
{
  oscpm::AddressSpace<float> levels;

  // Add a level for each dimmer, before either thread starts
  for (std::size_t dimmer = 1; dimmer <= 24; ++dimmer)
  {
      char address[32];
      std::snprintf(address, sizeof address, "/light/%02zu/level", dimmer);
      levels.add(*oscpm::Address::parse(address), 0.0f);
  }

  // Start the network thread, which pushes each received message, and the output thread, which calls outputFrame(levels) on its frame timer
  return 0;
}
```

## Matching Rules

A pattern and an address are split into parts on `/`. Both must have the same number of parts, except where `//` applies, and every pattern part must match the address part in the same position.

| Syntax | Meaning | Example | Matches | Does not match |
| --- | --- | --- | --- | --- |
| `?` | any one byte | `/synth/?` | `/synth/1` | `/synth/10` |
| `*` | any run of zero or more bytes | `/synth*` | `/synth`, `/synth1` | `/synth/1` |
| `[abc]`, `[a-z]` | one byte from the class; `a-z` is an inclusive ASCII range | `/synth/[1-3]` | `/synth/2` | `/synth/4` |
| `[!abc]` | one byte not in the class | `/synth/[!1-3]` | `/synth/4` | `/synth/2` |
| `{foo,bar}` | exactly one of the listed strings, taken literally | `/synth/{freq,amp}` | `/synth/amp` | `/synth/pan` |
| `//` | zero or more whole parts | `/a//c` | `/a/c`, `/a/b/c` | `/ab/c` |
| anything else | itself | `/synth` | `/synth` | `/Synth` |

Where the [OSC 1.0 specification](https://opensoundcontrol.stanford.edu/spec-1_0.html) leaves a case open, oscpm does the following.

### Parts and Slashes

- A wildcard, class or brace list never matches `/`: `/a*` does not match `/a/b`.
- A run of two or more slashes anywhere is one `//`, and the part before it must match whole:

  | Pattern | Matches |
  | --- | --- |
  | `//a` | `a` at any depth: `/a`, `/x/a`, `/x/y/a` |
  | `/a///b` | whatever `/a//b` matches |
  | `/a//` | `/a` and every address below it |
  | `//` | every address |

- A single trailing `/` is an empty part that only an address ending in `/` satisfies, so `/a/` does not match `/a`. The bare `/` likewise matches only the address `/`.

### Classes

A `-` between two characters is a range; first or last it is a member. Only a leading `!` negates.

| Class | Matches |
| --- | --- |
| `[--a]` | one byte in the range `-` to `a` |
| `[a-]`, `[-a]` | `a` or `-` |
| `[a-c-e]` | one byte in the range `a` to `c`, or `-`, or `e` |
| `[z-a]` | nothing: a reversed range is empty |
| `[]` | nothing |
| `[!]` | any one byte |
| `[a!]` | `a` or `!` |

### Brace Lists

- Inside `{` and `}` every byte is literal, `,` always delimits and the first `}` closes: `{a,{b,c}}` is the members `a`, `{b` and `c` followed by a literal `}`.
- An empty member matches the empty string: `{a,}` matches `a` or nothing.

### Other Bytes

- A `]`, `}` or `,` outside its construct, a `#`, a space and any byte outside printable ASCII is a literal that matches only itself. No well-formed address contains one, so a part holding one matches no well-formed address. In a brace list only the member holding it is affected: `{a b,c}` still matches `c`.
- Matching is by byte and case-sensitive, and the address is not validated.

### Malformed Patterns

`Pattern::parse` rejects a pattern only for one of these faults:

| Fault | `PatternError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a `[` with no `]` before the next `/` | `UnterminatedClass` |
| a `{` with no `}` before the next `/` | `UnterminatedBraces` |
| a wildcard, class, brace list or `//` in a pattern longer than `kMaxPatternLength` | `PatternTooLong` |

Every other pattern parses. A rejected pattern matches nothing: `match` returns false, and `AddressSpace::visit` and `dispatch` reach no value.

### Malformed Addresses

`Address::parse` rejects an address only for one of these faults:

| Fault | `AddressError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a final `/`, including the bare `/` | `TrailingSlash` |
| two adjacent slashes | `EmptyPart` |
| a byte outside printable ASCII, or one of `space`, `#`, `*`, `,`, `?`, `[`, `]`, `{`, `}` | `IllegalByte` |
| a part longer than `kMaxAddressPartLength` | `PartTooLong` |

Every well-formed address also parses as a literal pattern that matches only itself.

[`corpus/matching.txt`](corpus/matching.txt) is the executable record of these rules: one case per line, replayed by the test suite, in a plain-text format other implementations can reuse.

## Guarantees

`match`, `Pattern::parse`, `Pattern::matches`, `Address::parse` and `AddressSpace::find`:

- allocate nothing, which the test suite asserts with a counting `operator new`;
- never throw, and are declared `noexcept`;
- run in time bounded by the product of the pattern and address lengths, whatever the pattern contains;
- use an amount of stack that is fixed when they are compiled, whatever the pattern and address;
- are `constexpr` outside the address space, so a fixed pattern is parsed or matched at compile time.

`AddressSpace::visit` and `dispatch` allocate nothing and never throw, apart from whatever the visitor or handler they call does; they are not declared `noexcept`. Matching a pattern against the space takes at most the time bound above for each registered address. `AddressSpace::add` and `remove` allocate.

The stack a visit or dispatch uses is likewise fixed when it is compiled, and does not depend on the pattern, the address or the number of registered addresses: matching, visiting and dispatching never recurse, and size every stack buffer at compile time. With AppleClang 21 and GCC 14 at `-O3`, visiting a wildcard pattern typically uses under 2.5 KiB, plus 4 bytes for each of `InlineResults` when `Memo` is true (under 6.5 KiB with the defaults), and up to 1 KiB more for an address part of 64 bytes or longer. A visit served from the memo uses under 200 bytes.

A libFuzzer target checks the matcher, the pattern value and `Address::parse` against each other under AddressSanitizer and UndefinedBehaviorSanitizer on every change.

## Performance

- Processor: Apple M4
- Operating system: macOS 26.5.2
- Compiler: AppleClang 21.0.0.21000101, `-O3 -DNDEBUG`
- oscpm: v0.3.6, measured 2026-09-24
- Method: median wall-clock time of 10 repetitions; dispatch is into a space of 1,856 addresses

**Repeated visit of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 5.51 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq`, matching 128 addresses | 50.1 |
| `//freq`, matching 512 addresses | 154 |
| One message of a repeating stream of 64 | 10.4 |

**First visit of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 5.12 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq` | 88,600 |
| `//freq` | 32,000 |

**One pattern against one address**

| Operation | Median (ns) |
|---|---:|
| `Pattern::matches`, literal | 1.3 |
| `Pattern::matches`, wildcards | 100 |
| `oscpm::match` (parse and match), wildcards | 125 |

**Hostile patterns**

| Pattern | Median (us) |
|---|---:|
| The slowest pattern of 1024 bytes (`list-alternatives`) | 5,000 |
| A 64 KB wildcard pattern, rejected | 2.96 |

- A repeated message costs one hash lookup plus one visitor call per matched address. A literal address costs the same the first time.
- The first visit of any other pattern tests every registered address, so its cost grows with the size of the space.
- A wildcard pattern longer than `kMaxPatternLength` is rejected without being matched, which bounds the cost of a hostile pattern.
- The times are from one machine; the ratios between rows carry across machines better than the absolute figures.

### Running the Benchmarks

```
cmake --preset bench
cmake --build --preset bench
build/bench/bench/oscpm_bench
```

`python3 bench/compare.py report` builds the benchmarks and prints the setup and tables above.

## Integration

CI builds and tests oscpm with GCC and Clang on Linux, AppleClang on macOS and MSVC on Windows.

With CMake 3.26 or later, any of these gives the target `oscpm::oscpm`:

- **FetchContent**, pinned to one of the [release tags](https://github.com/jamiebullock/oscpm/tags):

  ```cmake
  include(FetchContent)
  FetchContent_Declare(oscpm
      GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
      GIT_TAG        <tag>)
  FetchContent_MakeAvailable(oscpm)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

- **`find_package`** after `cmake --install`, which installs the headers and a package config with a version file:

  ```cmake
  find_package(oscpm REQUIRED)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

  While the major version is 0, a version passed to `find_package` is satisfied only by an install of the same minor version; from 1.0, by one of the same major version.

- **`add_subdirectory`** on a checkout or a vendored copy. A source archive has no git history to read the version from, so pass `-DOSCPM_VERSION=<version>` when configuring it.

Without CMake, put `include/` on the include path.

`oscpm/pattern.h` holds the matcher, `oscpm/address.h` the address value, `oscpm/address_space.h` the address space and `oscpm/error.h` the faults they report; `oscpm/oscpm.h` includes all four. Nothing under `oscpm/detail/` is public API.

## Building

Builds go through `CMakePresets.json`:

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja, `windows` uses Visual Studio 2022 and `bench` is `release` with the benchmarks. `mutation` is `release` with the tests compiled under clang-18 with the [Mull](https://github.com/mull-project/mull) plugin, and its build runs them once per mutant; it needs Mull for LLVM 18 on Linux. `DEPENDENCIES.md` lists what is fetched at configure time and why.

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | builds the tests and fetches doctest; `OFF` registers nothing with CTest. Ignored when oscpm is a subproject, which never builds the tests |
| `OSCPM_BUILD_EXAMPLES` | `ON` at the top level, `OFF` as a subproject | builds the example and fetches oscpp |
| `OSCPM_CHECK_FORMAT` | `ON` at the top level, `OFF` as a subproject | fails the build on a clang-format violation; needs clang-format |
| `OSCPM_BUILD_BENCHMARKS` | `OFF` | builds the benchmarks and fetches Google Benchmark; the `bench` preset sets it |
| `OSCPM_BUILD_FUZZERS` | `OFF` | builds a libFuzzer target under AddressSanitizer and UndefinedBehaviorSanitizer, seeded from the corpus; needs an LLVM clang |
| `OSCPM_SANITIZE` | `OFF` | builds the tests under the same sanitizers |
| `OSCPM_MULL_PLUGIN` | empty | the Mull IR plugin to compile the tests with; adds the `mutation` target, which fails when the mutation score is below `OSCPM_MUTATION_THRESHOLD`. The `mutation` preset sets it |

## Versioning

oscpm follows [Semantic Versioning](https://semver.org/). Each release is a `vX.Y.Z` [tag](https://github.com/jamiebullock/oscpm/tags).

## Licence

zlib. See [`LICENSE`](LICENSE).
