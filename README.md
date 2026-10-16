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

`visit` and `dispatch` take the pattern as text and return a `MatchResult` holding:
- `matched`, the number of values visited or called
- `error`, the `PatternError` when the pattern was malformed

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

### Brackets and Braces

- Following an opening `[`, the first `]` closes it
- Following an opening `{`, the first `}` closes it
- Inside `{` and `}`:
    * Every byte is literal and `,` always delimits, so `{a,{b,c}}` is the members `a`, `{b` and `c` followed by a literal `}`
    * An empty member matches the empty string: `{a,}` matches `a` or nothing
- Inside `[` and `]`:
    * Every byte is a member except a leading `!` and a `-` between two bytes
    * `[a*]b]` is the class `a` or `*` followed by the literals `b` and `]`

### Other Bytes

The following are treated as literals:
- `#`, space, and all control characters except null
- `]`, `}`, and `,` outside a class or brace list

None of the above bytes are valid in a well-formed address, so they never match:
- `/a b` matches nothing
- `/a}` matches nothing
  
Inside a class or brace list, only the member containing the byte is excluded:
- `/{a b,c}` matches `/c`
- `/[a ]` matches `/a`

### Malformed Patterns

The following patterns are considered malformed and will be rejected by `Pattern::parse()` and cause the `Pattern` constructor to abort:

| Fault | `PatternError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a `[` with no `]` before the next `/` | `UnterminatedClass` |
| a `{` with no `}` before the next `/` | `UnterminatedBraces` |
| a null or a byte above 0x7F | `IllegalByte` |
| a wildcard, class, brace list or `//` in a pattern longer than `kMaxPatternLength` | `PatternTooLong` |

### Malformed Addresses

The following addresses are considered malformed and will be rejected by `Address::parse()` and cause the `Address` constructor to abort:

| Fault | `AddressError` |
| --- | --- |
| no leading `/` | `MissingLeadingSlash` |
| a final `/`, including the bare `/` | `TrailingSlash` |
| two adjacent slashes | `EmptyPart` |
| a byte outside printable ASCII, or one of `space`, `#`, `*`, `,`, `?`, `[`, `]`, `{`, `}` | `IllegalByte` |
| a part longer than `kMaxAddressPartLength` | `PartTooLong` |

### Conformance Corpus

[`corpus/matching.txt`](corpus/matching.txt) is the executable record of every rule in this section. It's structured one case per line and replayed by the test suite. It is in a plain-text format other implementations can reuse.

## Guarantees

Nothing in the library allocates except `AddressSpace` construction, copying, `add` and `remove`.

Nothing in the library calls `std::abort` except the `Address` and `Pattern` constructors on malformed text, and `Expected` dereferenced while holding an error or `Expected::error` called while holding a value.

`match`, `Pattern::parse`, `Pattern::matches`, `Address::parse` and `AddressSpace::find`:
- never throw, and are declared `noexcept`
- have execution time bounded by the product of the pattern and address lengths
- have stack size fixed at compile time
- are `constexpr` outside the address space, so a fixed pattern can be parsed or matched at compile time

`AddressSpace::visit` and `AddressSpace::dispatch`:
- only propagate exceptions thrown by the visitor or handler they call
- have execution time bounded by the pattern match time multiplied by the number of registered addresses
- have a stack size fixed at compile time
- use only a single lookup for literal and memoised patterns

Every code change can only merge after the test suite and [conformance corpus](#conformance-corpus) have run under `AddressSanitizer` and `UndefinedBehaviorSanitizer`. This guarantees the following for the tested code paths:
- no out-of-bounds or use-after-free memory access
- no undefined behaviour

## CMake Setup

With CMake 3.26 or later, any of these provides the target `oscpm::oscpm`:

- **FetchContent**, pinned to one of the [release tags](https://github.com/jamiebullock/oscpm/tags):

  ```cmake
  include(FetchContent)
  FetchContent_Declare(oscpm
      GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
      GIT_TAG        <tag>)
  FetchContent_MakeAvailable(oscpm)
  target_link_libraries(app PRIVATE oscpm::oscpm)
  ```

- **`find_package`** after `cmake --install`, which installs the headers and a package config
- **`add_subdirectory`** on a checkout or a vendored copy

A version passed to `find_package` is satisfied only by an install of the same minor version while the major version is 0, and by one of the same major version from 1.0.

Without CMake, put `include/` on the include path. `oscpm/oscpm.h` includes every public header. Nothing under `oscpm/detail/` is public API.

## Supported Platforms

oscpm supports the following platforms:

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

The rows marked CI are built on every push and pull request to `develop`.

Each Arduino row compiles the sketch in `tests/arduino` at the core's default flags, apart from the two whose cores default to an earlier C++ standard. On the UNO R4, whose libstdc++ 7 cannot measure a string literal in a constant expression, a compile-time call takes a `std::string_view` built with an explicit length; the sketch shows one way to write that.

## Performance

[oscpm-bench](https://github.com/jamiebullock/oscpm-bench) times oscpm against other OSC pattern matchers on the cases its README defines:

- Processor: Apple M4
- Operating system: macOS 27.0.1
- Compiler: AppleClang 21.0.0.21000334, `-O3 -DNDEBUG`
- oscpm: v0.9.8-17-gda271e0, measured 2026-10-15
- Comparators: liblo 0.36 (025f277), libo master, 2023-07-16 (874edd2), CNMAT OSC 4.0.0 (d0a965d), rtosc v0.3.1 (f3882fe), oscpkt 1.2, JUCE 9.0.4 (3fb5cbc)
- Method: median wall-clock time in nanoseconds of 10 repetitions; a ratio is the comparator's time over oscpm's, and the geometric mean is over the cases both took, leaving out the backtracking case

**Two strings in, one bool out**

| Case | oscpm::match | liblo | libo | CNMAT OSC | rtosc | oscpkt |
|---|---:|---:|---:|---:|---:|---:|
| literal | 53.0 | 25.7 (0.5x) | 50.0 (0.9x) | 7.29 (0.1x) | 39.9 (0.8x) | 17.9 (0.3x) |
| question | 59.6 | 13.7 (0.2x) | 24.6 (0.4x) | 32.1 (0.5x) | - | 9.85 (0.2x) |
| star | 99.2 | 27.3 (0.3x) | 52.4 (0.5x) | 62.5 (0.6x) | 40.2 (0.4x) | 21.1 (0.2x) |
| class | 123 | 26.6 (0.2x) | 49.9 (0.4x) | 62.4 (0.5x) | - | 19.9 (0.2x) |
| list | 129 | 32.3 (0.3x) | 60.6 (0.5x) | 69.3 (0.5x) | 39.7 (0.3x) | 26.4 (0.2x) |
| descendant | 61.1 | 27.4 (0.4x) | - | - | - | 28.7 (0.5x) |
| combined | 152 | 35.0 (0.2x) | 58.5 (0.4x) | 69.1 (0.5x) | - | 29.5 (0.2x) |
| combined-miss | 148 | 58.4 (0.4x) | 55.5 (0.4x) | 64.5 (0.4x) | - | 28.0 (0.2x) |
| backtracking | 146 | 239,297 (1635.3x) | 452,756 (3094.0x) | 553,349 (3781.5x) | - | 466,663 (3189.1x) |
| geometric mean | 1.00x | 0.30x over 8 | 0.48x over 7 | 0.43x over 7 | 0.46x over 3 | 0.23x over 8 |

**Parsed before the timed loop**

| Case | Pattern::matches | JUCE |
|---|---:|---:|
| literal | 1.50 | 3.29 (2.2x) |
| question | 34.4 | 24.4 (0.7x) |
| star | 53.3 | 44.2 (0.8x) |
| class | 83.1 | 63.0 (0.8x) |
| list | 74.9 | 173 (2.3x) |
| descendant | 40.4 | - |
| combined | 99.1 | 189 (1.9x) |
| combined-miss | 99.4 | 187 (1.9x) |
| backtracking | 124 | 579,089 (4676.3x) |
| geometric mean | 1.00x | 1.35x over 7 |

**Dispatch into a space of 1,856 addresses**

| Case | AddressSpace | liblo | rtosc |
|---|---:|---:|---:|
| literal | 3.76 | 1,403 (373.6x) | 105 (27.8x) |
| pattern | 87,722 | 61,948 (0.7x) | - |
| pattern-repeated | 46.9 | - | - |

What the tables show:

- `oscpm::match` validates both strings and parses the pattern on every call; the other matchers in its table take the strings as they are. `Pattern::matches` and JUCE work on objects their library has already validated
- The time of a pattern is bounded by its length times the address length, so the backtracking case costs oscpm about the same as the combined case. The other matchers backtrack, and that case costs them thousands of times more
- A literal address dispatched into the space is one hash lookup. liblo deserialises the message and compares it with each registered method in turn; rtosc hashes each path segment in turn
- The first dispatch of a pattern tests every registered address, so its cost grows with the size of the space. The second is answered from the memo, which neither comparator has
- A blank is a case the matcher does not take or answers differently: libo, CNMAT OSC, rtosc and JUCE have no `//`; rtosc takes `?` and `[` as literal bytes and `*` only at the end of a part, and dispatches a literal address only

### Running the Benchmarks

```
cmake --preset bench
cmake --build --preset bench
build/bench/bench/oscpm_bench
```

`python3 bench/compare.py report` builds the in-tree benchmarks and prints the setup and their medians as Markdown. The tables above are produced by `report.py` in [oscpm-bench](https://github.com/jamiebullock/oscpm-bench).

## Building

Builds go through `CMakePresets.json`:

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja, `windows` uses Visual Studio 2022 and `bench` is `release` with the benchmarks. `mutation` is `release` with the tests compiled under clang-18 with the [Mull](https://github.com/mull-project/mull) plugin, and its build runs them once per mutant; it needs Mull for LLVM 18 on Linux. `DEPENDENCIES.md` lists what is fetched at configure time and why.

The version is read from the most recent git tag. A source archive has no git history, so pass `-DOSCPM_VERSION=<version>` when configuring.

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
