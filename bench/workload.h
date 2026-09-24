/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace oscpm_bench
{

constexpr std::size_t kNumSpaces = 2;

/// A registered address space of synths, voices and mixer channels.
struct Space
{
    const char* name;
    std::size_t index;
    std::vector<std::string> addresses;
};

/// The spaces every dispatch benchmark runs against: "small" (256 addresses)
/// and "large" (1,856), with `index` their position in this list.
const std::array<Space, kNumSpaces>& spaces();

/// A musical pattern and the number of addresses it matches in each space,
/// by `Space::index`.
struct MusicalPattern
{
    const char* id;
    const char* text;
    std::array<std::size_t, kNumSpaces> numMatches;
};

const std::vector<MusicalPattern>& musicalPatterns();

/// A pattern, an address and whether the one matches the other.
struct MatchPair
{
    const char* id;
    const char* pattern;
    const char* address;
    bool matches;
};

const std::vector<MatchPair>& matchPairs();

/// A hostile pattern, generated as `prefix`, then `unit` repeated, then
/// `suffix`. None matches an address of either space.
struct AdversarialShape
{
    const char* id;
    const char* prefix;
    const char* unit;
    const char* suffix;
};

const std::vector<AdversarialShape>& adversarialShapes();

/// `shape` with as many repetitions of its unit as fit in `length` bytes.
std::string adversarialPattern(const AdversarialShape& shape, std::size_t length);

/// The largest pattern one UDP datagram can carry in an OSC message.
constexpr std::size_t kDatagramPatternLength = 65495;

/// A cycle of messages to `space`: every musical pattern, each followed by
/// literal addresses drawn evenly from the space.
std::vector<std::string> messageStream(const Space& space);

/// The number of methods one pass over `messageStream(space)` visits.
std::size_t numStreamMatches(const Space& space);

}
