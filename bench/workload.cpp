/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "workload.h"

#include <string_view>

namespace oscpm_bench
{

namespace
{

    namespace k
    {
        constexpr std::array<std::string_view, 4> oscillators = { "saw", "square", "sine", "tri" };
        constexpr std::array<std::string_view, 3> oscillatorParameters = { "freq", "gain", "detune" };
        constexpr std::array<std::string_view, 2> filterParameters = { "cutoff", "res" };
        constexpr std::array<std::string_view, 2> synthParameters = { "gain", "pan" };
        constexpr std::array<std::string_view, 3> channelParameters = { "gain", "pan", "mute" };
        constexpr std::size_t numLiteralsPerPattern = 7;
    }

    struct SpaceShape
    {
        std::size_t numSynths;
        std::size_t numVoices;
        std::size_t numChannels;
    };

    constexpr SpaceShape kSmallShape { 4, 4, 8 };
    constexpr SpaceShape kLargeShape { 8, 16, 16 };

    std::vector<std::string> addressesOf(const SpaceShape& shape)
    {
        std::vector<std::string> addresses;
        for (std::size_t synth = 1; synth <= shape.numSynths; ++synth)
        {
            const std::string synthPrefix = "/synth" + std::to_string(synth);
            for (const std::string_view parameter : k::synthParameters)
            {
                addresses.push_back(synthPrefix + "/" + std::string(parameter));
            }
            for (std::size_t voice = 1; voice <= shape.numVoices; ++voice)
            {
                const std::string voicePrefix = synthPrefix + "/voice/" + std::to_string(voice);
                for (const std::string_view oscillator : k::oscillators)
                {
                    for (const std::string_view parameter : k::oscillatorParameters)
                    {
                        addresses.push_back(voicePrefix + "/osc/" + std::string(oscillator) + "/" + std::string(parameter));
                    }
                }
                for (const std::string_view parameter : k::filterParameters)
                {
                    addresses.push_back(voicePrefix + "/filter/" + std::string(parameter));
                }
            }
        }
        for (std::size_t channel = 1; channel <= shape.numChannels; ++channel)
        {
            for (const std::string_view parameter : k::channelParameters)
            {
                addresses.push_back("/mixer/ch" + std::to_string(channel) + "/" + std::string(parameter));
            }
        }
        return addresses;
    }

}

const std::array<Space, kNumSpaces>& spaces()
{
    static const std::array<Space, kNumSpaces> all = {
        Space { "small", 0, addressesOf(kSmallShape) },
        Space { "large", 1, addressesOf(kLargeShape) },
    };
    return all;
}

const std::vector<MusicalPattern>& musicalPatterns()
{
    static const std::vector<MusicalPattern> all = {
        { "example", "/synth[3-6]/voice/*/osc/{saw,square}/freq", { 16, 128 } },
        { "literal", "/synth3/voice/7/osc/saw/freq", { 0, 1 } },
        { "filter-all-voices", "/synth2/voice/*/filter/cutoff", { 4, 16 } },
        { "all-synths-osc-gain", "/synth*/voice/1/osc/*/gain", { 16, 32 } },
        { "mixer-channel", "/mixer/ch?/gain", { 8, 9 } },
        { "descendant-freq", "//freq", { 64, 512 } },
        { "voice-list", "/synth[1-8]/voice/{1,2,3,4}/osc/sine/detune", { 16, 32 } },
        { "miss", "/synth9/voice/*/osc/saw/freq", { 0, 0 } },
    };
    return all;
}

const std::vector<MatchPair>& matchPairs()
{
    static const std::vector<MatchPair> all = {
        { "example-hit", "/synth[3-6]/voice/*/osc/{saw,square}/freq", "/synth4/voice/12/osc/square/freq", true },
        { "example-miss-last", "/synth[3-6]/voice/*/osc/{saw,square}/freq", "/synth4/voice/12/osc/square/gain", false },
        { "example-miss-first", "/synth[3-6]/voice/*/osc/{saw,square}/freq", "/synth8/voice/12/osc/square/freq", false },
        { "literal-hit", "/synth3/voice/7/osc/saw/freq", "/synth3/voice/7/osc/saw/freq", true },
    };
    return all;
}

const std::vector<AdversarialShape>& adversarialShapes()
{
    static const std::vector<AdversarialShape> all = {
        { "stars", "/", "*a", "*b" },
        { "descendant-stars", "//", "*a", "*b" },
        { "descendant-chain", "", "//*", "" },
        { "question-run", "/", "?", "" },
        { "class-members", "/[", "a", "]" },
        { "class-run", "/", "[a-z]", "" },
        { "list-alternatives", "/{", "ab,", "c}" },
        { "list-run", "/", "{a,aa}", "" },
        { "many-parts", "", "/*", "" },
        { "musical-then-stars", "/synth[3-6]/voice/*/osc/", "*a", "*b/freq" },
    };
    return all;
}

std::string adversarialPattern(const AdversarialShape& shape, std::size_t length)
{
    const std::string_view prefix = shape.prefix;
    const std::string_view unit = shape.unit;
    const std::string_view suffix = shape.suffix;
    const std::size_t numUnits = (length - prefix.size() - suffix.size()) / unit.size();
    std::string pattern(prefix);
    pattern.reserve(length);
    for (std::size_t i = 0; i < numUnits; ++i)
    {
        pattern += unit;
    }
    pattern += suffix;
    return pattern;
}

std::vector<std::string> messageStream(const Space& space)
{
    const std::vector<MusicalPattern>& patterns = musicalPatterns();
    const std::size_t numLiterals = patterns.size() * k::numLiteralsPerPattern;
    const std::size_t stride = space.addresses.size() / numLiterals;
    std::vector<std::string> messages;
    std::size_t nextLiteral = 0;
    for (const MusicalPattern& pattern : patterns)
    {
        messages.emplace_back(pattern.text);
        for (std::size_t i = 0; i < k::numLiteralsPerPattern; ++i)
        {
            messages.push_back(space.addresses[nextLiteral * stride]);
            ++nextLiteral;
        }
    }
    return messages;
}

std::size_t numStreamMatches(const Space& space)
{
    std::size_t numMatches = 0;
    for (const MusicalPattern& pattern : musicalPatterns())
    {
        numMatches += pattern.numMatches[space.index] + k::numLiteralsPerPattern;
    }
    return numMatches;
}

}
