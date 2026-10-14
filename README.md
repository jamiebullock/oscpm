# oscpm

Header-only C++17 OpenSoundControl (OSC) pattern matching and address space.

[![CI](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/jamiebullock/oscpm/actions/workflows/ci.yml)
[![Latest tag](https://img.shields.io/github/v/tag/jamiebullock/oscpm?sort=semver&label=tag)](https://github.com/jamiebullock/oscpm/tags)
[![Licence](https://img.shields.io/github/license/jamiebullock/oscpm?label=licence)](LICENSE)

## Features

- **Compliant:** full [OSC 1.0](https://opensoundcontrol.stanford.edu/spec-1_0.html) pattern syntax (`?`, `*`, `[...]`, `{a,b}`) plus the [OSC 1.1](https://opensoundcontrol.stanford.edu/spec-1_1.html) `//` operator
- **Address space:** register handlers or values, then `visit` or `dispatch` every match of a received pattern, with repeated patterns served from a memo
- **Allocation-free, non-throwing** matching and dispatch, usable on a realtime thread
- **Bounded time:** at most pattern length x address length whatever the pattern; no backtracking, no recursion
- **Fixed stack:** every buffer sized at compile time, independent of the pattern, the address and the number of registered addresses
- **Strict validation:** a malformed pattern or address is reported as a `PatternError` or `AddressError`, never matched by accident
- **`constexpr` parsing and matching:** a literal that does not parse is a compile error
- **Header-only, C++17, standard library only:** builds on Linux, macOS, Windows and Arduino cores ([Supported Platforms](#supported-platforms))

oscpm pairs well with any packet library that leaves matching to the caller: [oscpp](https://github.com/kaoskorobase/oscpp), [oscpack](https://github.com/RossBencina/oscpack), [tinyosc](https://github.com/mhroth/tinyosc), and [MicroOsc](https://github.com/thomasfredericks/MicroOsc). See [Integration Examples](#integration-examples).

## Matching
In the simple case a match can be checked by comparing two `std::string_view`:

```cpp
#include <oscpm/pattern.h>

oscpm::match("/synth/*/freq", "/synth/1/freq"); // true
oscpm::match("/synth//freq", "/synth/1/osc/freq"); // true (OSC 1.1 syntax)
oscpm::match("/synth/[1-3]/{freq,amp}", "/synth/2/amp"); // true
oscpm::match("/synth/*/freq", "/1/synth/freq"); // false (no match found)
oscpm::match("synth/*/freq", "1/synth/freq"); // false (address and pattern are not valid)
```

> `oscpm::match()` returns `true` if the address and pattern are well-formed and the address matches the pattern. If the caller needs to distinguish validity from match success, then [Pattern::parse()](#patterns) and [Address::parse()](#addresses) should be used instead.

## Patterns
A pattern that will be matched many times can be validated once and kept as a `Pattern` object. This has the following advantages:
- A malformed pattern is reported as a `PatternError` so the caller can distinguish an invalid pattern from a non-match
- For `constexpr` patterns, validity can be checked at compile time
- Matching with `Pattern::matches()` is faster than `oscpm::match()` because the pattern is validated once, and patterns without wildcards reduce to a plain string comparison

A `Pattern` is constructed from text the program controls, and aborts if the text is malformed. For text from outside the program, `Pattern::parse()` returns the fault as a value instead:

```cpp
constexpr oscpm::Pattern pattern("/synth/*/{freq,amp}"); // a malformed literal fails to compile
static_assert(pattern.matches(oscpm::Address("/synth/12/amp")));

if (const auto result = oscpm::Pattern::parse(text))
{
    result->matches(address); // an oscpm::Address
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
- `Pattern::matches()` takes an `Address`, so a pattern is only ever matched against a well-formed address

```cpp
#include <oscpm/address.h>
#include <oscpp/client.hpp>

constexpr oscpm::Address address("/synth/1/freq"); // a malformed literal fails to compile
static_assert(oscpm::Pattern("/synth/*/freq").matches(address));

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

`AddressSpace<T>` associates OSC addresses with values of type `T`. Depending on the type chosen for `T`, an OSC method can correspond to a callback function or a parameter the address space itself manages.

### Registering Callbacks

In the common form:
- The caller registers a callback for each OSC address
- An OSC message is dispatched to the address space
- Every callback whose address matches the message's address pattern is called with the message as an argument

The mixer example below shows this:

```cpp
// A message received from the OSC client, e.g. { "/mixer/*/gain", 0.5f } or { "/mixer/3/mute", 1.0f }
struct Message
{
  std::string_view addressPattern;
  float argument;
};

// In a real application this would read the next incoming message
bool receive(Message& message);

// Application-side struct to hold the live channel settings
struct Channel
{
  float gain = 0.0f;
  bool mute = false;
};

int main()
{
  std::array<Channel, 8> channels;
  oscpm::AddressSpace<std::function<void(const Message&)>> handlers;

  // Add a gain and a mute handler for each channel
  for (std::size_t index = 0; index < channels.size(); ++index)
  {
      Channel& channel = channels[index];
      const std::string prefix = "/mixer/" + std::to_string(index + 1);
      
      handlers.add(oscpm::Address(prefix + "/gain"), [&channel](const Message& message) { channel.gain = message.argument; });
      handlers.add(oscpm::Address(prefix + "/mute"), [&channel](const Message& message) { channel.mute = message.argument != 0.0f; });
  }

  // The main receive loop
  Message message;
  while (receive(message))
  {
      // Dispatch the OSC message to every method whose address matches the received pattern, invoking the corresponding handler on each match
      const oscpm::MatchResult result = handlers.dispatch(message.addressPattern, message);
      if (result.error)
      {
          std::cerr << "malformed pattern " << message.addressPattern << ": " << oscpm::toString(*result.error) << '\n';
      }
      else if (result.matched == 0)
      {
          std::cerr << "no channel at " << message.addressPattern << '\n';
      }
  }

  // Print the new values set by the handlers
  for (std::size_t index = 0; index < channels.size(); ++index)
  {
      std::cout << index + 1 << ' ' << channels[index].gain << (channels[index].mute ? " muted" : "") << '\n';
  }
  return 0;
}
```

### Registering Parameters

In the alternative form, the address space stores the parameters directly.

This suits applications where:
- Every address holds the same kind of value
- Only the latest value is needed, not every intermediate one

Used as follows:
- The caller registers a _value_ for each OSC address
- A visitor is called for each address matching an incoming OSC pattern
- The visitor can then set or get the value

The lighting rig example below shows this:

```cpp
// A message received from the lighting console, e.g. { "/light/0[1-4]/level", 0.75f } or { "/light/*/level", 0.0f }
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
  std::cout << "dimmer 3: " << *levels.find("/light/03/level") << '\n';

  // Or read every level in address order to build the frame
  levels.visit([](std::string_view address, float level)
      { std::cout << address << ' ' << level << '\n'; });
}

int main()
{
  constexpr std::size_t numDimmers = 24;
  oscpm::AddressSpace<float> levels;

  // Add a level for each dimmer, before either thread starts
  for (std::size_t dimmer = 1; dimmer <= numDimmers; ++dimmer)
  {
      char text[32];
      std::snprintf(text, sizeof text, "/light/%02zu/level", dimmer);
      levels.add(oscpm::Address(text), 0.0f);
  }

  // Start the network thread, which pushes each received message, and the output thread, which calls outputFrame(levels) on its frame timer
  return 0;
}
```

## Integration Examples

The following examples show how to integrate oscpm with common OSC packet libraries.

### oscpp

[oscpp](https://github.com/kaoskorobase/oscpp) hands each message to the caller as an `OSCPP::Server::Message`, whose `address()` is the pattern to dispatch.

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
methods.add(oscpm::Address("/synth/freq"), setFrequency);

// for each OSCPP::Server::Message received
methods.dispatch(message.address(), message); // A message with address "/synth/*" calls setFrequency()
```

### oscpack

[oscpack](https://github.com/RossBencina/oscpack) delivers messages through a subclass of `osc::OscPacketListener`, which also unpacks bundles. `Handler` is a `std::function<void(const osc::ReceivedMessage&)>`.

```cpp
#include <osc/OscPacketListener.h>

void setFrequency(const osc::ReceivedMessage& message)
{
    float frequency;
    message.ArgumentStream() >> frequency >> osc::EndMessage;
    synth.setFrequency(frequency);
}

class Listener : public osc::OscPacketListener
{
    void ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName&) override
    {
        methods.dispatch(message.AddressPattern(), message); // A message with address "/synth/*" calls setFrequency()
    }
};
```

### tinyosc

[tinyosc](https://github.com/mhroth/tinyosc) is a C library that parses a datagram in place. A `tosc_message` is a read cursor over the arguments, so each method takes its own copy and reads from the first argument. `Handler` is a `std::function<void(tosc_message)>`.

```cpp
#include <tinyosc.h>

void setFrequency(tosc_message message)
{
    synth.setFrequency(tosc_getNextFloat(&message));
}

// for each datagram of length bytes received into buffer
tosc_message message;
if (tosc_parseMessage(&message, buffer, length) == 0)
{
    methods.dispatch(tosc_getAddress(&message), message); // A message with address "/synth/*" calls setFrequency()
}
```

### MicroOsc

[MicroOsc](https://github.com/thomasfredericks/MicroOsc) is an Arduino library that receives over UDP or SLIP serial and calls a plain function for each message. As with tinyosc, a `MicroOscMessage` is a read cursor, so each method takes a copy. `Handler` is a function pointer, `void (*)(MicroOscMessage)`.


```cpp
#include <MicroOscSlip.h>

MicroOscSlip<128> osc(&Serial);

void setFrequency(MicroOscMessage message)
{
    synth.setFrequency(message.nextAsFloat());
}

void onMessage(MicroOscMessage& message)
{
    methods.dispatch(message.getOscAddress(), message); // A message with address "/synth/*" calls setFrequency()
}

void loop()
{
    osc.onOscMessageReceived(onMessage);
}
```

## Matching Rules

- A pattern and an address are split into parts on `/`
- For a match:
    * Both must have the same number of parts, except where `//` applies
    * Comparison is by byte and case-sensitive
    * Each pattern part must match the address part in the same position, by these rules:
    
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

- A pattern ending in a single `/` matches nothing, since an address ending in `/` is [malformed](#malformed-addresses). `/a/` and `/` therefore match no address.

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


### Malformed Patterns

`Pattern::parse` rejects a pattern, and the `Pattern` constructor aborts, only for one of these faults:

| Fault | `PatternError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a `[` with no `]` before the next `/` | `UnterminatedClass` |
| a `{` with no `}` before the next `/` | `UnterminatedBraces` |
| a wildcard, class, brace list or `//` in a pattern longer than `kMaxPatternLength` | `PatternTooLong` |

Every other pattern parses. A rejected pattern matches nothing: `match` returns false, and `AddressSpace::visit` and `dispatch` reach no value.

### Malformed Addresses

`Address::parse` rejects an address, and the `Address` constructor aborts, only for one of these faults:

| Fault | `AddressError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a final `/`, including the bare `/` | `TrailingSlash` |
| two adjacent slashes | `EmptyPart` |
| a byte outside printable ASCII, or one of `space`, `#`, `*`, `,`, `?`, `[`, `]`, `{`, `}` | `IllegalByte` |
| a part longer than `kMaxAddressPartLength` | `PartTooLong` |

A rejected address matches nothing: `match` returns false, and `Pattern::matches` takes only a parsed `Address`. Every well-formed address also parses as a literal pattern that matches only itself.

[`corpus/matching.txt`](corpus/matching.txt) is the executable record of these rules: one case per line, replayed by the test suite, in a plain-text format other implementations can reuse.

## Guarantees

`match`, `Pattern::parse`, `Pattern::matches`, `Address::parse` and `AddressSpace::find`:

- allocate nothing, which the test suite asserts with a counting `operator new`;
- never throw, and are declared `noexcept`;
- run in time bounded by the product of the pattern and address lengths, whatever the pattern contains;
- use an amount of stack that is fixed when they are compiled, whatever the pattern and address;
- are `constexpr` outside the address space, so a fixed pattern is parsed or matched at compile time.

The library calls `std::abort` in two places: the `Address` and `Pattern` constructors on malformed text, and `Expected` when the side it does not hold is read. Each is a programming error in the caller.

`AddressSpace::visit` and `dispatch` allocate nothing and never throw, apart from whatever the visitor or handler they call does; they are not declared `noexcept`. Matching a pattern against the space takes at most the time bound above for each registered address. `AddressSpace::add` and `remove` allocate.

The stack a visit or dispatch uses is likewise fixed when it is compiled, and does not depend on the pattern, the address or the number of registered addresses: matching, visiting and dispatching never recurse, and size every stack buffer at compile time. With AppleClang 21 and GCC 14 at `-O3`, visiting a wildcard pattern typically uses under 2.5 KiB, plus 4 bytes for each of `InlineResults` when `Memo` is true (under 6.5 KiB with the defaults), and up to 1 KiB more for an address part of 64 bytes or longer. A visit served from the memo uses under 200 bytes.

A libFuzzer target checks the matcher, the pattern value and `Address::parse` against each other under AddressSanitizer and UndefinedBehaviorSanitizer on every change.

## CMake Setup

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

## Supported Platforms

oscpm supports the following platforms, needing only a C++17 compiler and its standard library:

| Platform | Toolchain | Checked |
| --- | --- | --- |
| Linux | GCC, Clang | CI |
| macOS | AppleClang | CI |
| Windows | MSVC, Visual Studio 2022 | CI |
| Arduino UNO R4 Minima | `arduino:renesas_uno` core, GCC 7.2.1 with libstdc++ 7 | CI |
| ESP8266 | `esp8266:esp8266` core | CI |
| Raspberry Pi Pico | `rp2040:rp2040` core | CI |
| Raspberry Pi Pico 2 | `rp2040:rp2040` core | `arduino-cli`, 2026-10-10 |
| ESP32 | `esp32:esp32` core | `arduino-cli`, 2026-10-10 |
| STM32 Nucleo F401RE | `STMicroelectronics:stm32` core | `arduino-cli`, 2026-10-10 |
| Teensy LC, Teensy 4.1 | `teensy:avr` core | `arduino-cli`, 2026-10-10 |
| Arduino Giga R1, Arduino UNO Q | `arduino:zephyr_main` and `arduino:zephyr` cores | `arduino-cli`, 2026-10-10 |
| Arduino MKR Zero | `arduino:samd` core, with `-std=gnu++17` | `arduino-cli`, 2026-10-10 |
| Adafruit Feather nRF52840 | `adafruit:nrf52` core, with `-std=gnu++17` and `-lstdc++` | `arduino-cli`, 2026-10-10 |

The rows marked CI are built on every push and pull request to `develop`. Each Arduino row compiles the sketch in `tests/arduino` at the core's default flags, apart from the two whose cores default to an earlier C++ standard. The 8-bit AVR boards have no C++ standard library, and the `arduino:mbed_nano` core's `abs` macro breaks libstdc++ at C++17, so oscpm does not compile for them. On the UNO R4, whose libstdc++ 7 cannot measure a string literal in a constant expression, a compile-time call takes a `std::string_view` built with an explicit length; the sketch shows one way to write that.

## Performance

- Processor: Apple M4
- Operating system: macOS 27.0.1
- Compiler: AppleClang 21.0.0.21000334, `-O3 -DNDEBUG`
- oscpm: v0.9.5-1-gea3c93c, measured 2026-10-11
- Method: median wall-clock time of 10 repetitions; dispatch is into a space of 1,856 addresses

**Repeated visit of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 5.49 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq`, matching 128 addresses | 48.2 |
| `//freq`, matching 512 addresses | 148 |
| One message of a repeating stream of 64 | 10.2 |

**First visit of a pattern**

| Pattern | Median (ns) |
|---|---:|
| Literal address | 4.73 |
| `/synth[3-6]/voice/*/osc/{saw,square}/freq` | 82,800 |
| `//freq` | 31,400 |

**One pattern against one address**

| Operation | Median (ns) |
|---|---:|
| `Pattern::matches`, literal | 1.24 |
| `Pattern::matches`, wildcards | 91.2 |
| `oscpm::match` (parse and match), wildcards | 130 |

**Hostile patterns**

| Pattern | Median (us) |
|---|---:|
| The slowest pattern of 1024 bytes (`list-alternatives`) | 4,690 |
| A 64 KB wildcard pattern, rejected | 2.79 |

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

On a pull request, CI counts the instructions per iteration of every benchmark under Cachegrind for the change and for `develop`, and fails when a benchmark rises by more than 2% and 4 instructions. A change that accepts such a rise names the benchmark and its new ceiling, in instructions per iteration as the job's summary reports them, in `bench/accepted.json`. The check passes that benchmark while it stays at or below the ceiling and fails it again above, and it reports an entry that covers no rise against `develop`, which is the case once the change has merged, so the entry can be removed.

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

oscpm follows [Semantic Versioning](https://semver.org/). Each release is a `vX.Y.Z` [tag](https://github.com/jamiebullock/oscpm/tags) on `main`; changes land on `develop` between releases. `CHANGELOG.md` lists what each release changed, and [CONTRIBUTING.md](CONTRIBUTING.md) describes how one is made.

## Licence

zlib. See [`LICENSE`](LICENSE).
