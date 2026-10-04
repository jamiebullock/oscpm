/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "corpus.h"

#include <oscpm/oscpm.h>

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>

namespace
{

using oscpm_test::CorpusCase;
using oscpm_test::Expectation;

void checkCase(const CorpusCase& corpusCase)
{
    INFO("corpus line " << corpusCase.line << ": " << corpusCase.text);

    const oscpm::Pattern::ParseResult parsed = oscpm::Pattern::parse(corpusCase.pattern);
    const bool matched = oscpm::match(corpusCase.pattern, corpusCase.address);

    if (corpusCase.expectation == Expectation::MalformedPattern)
    {
        REQUIRE_FALSE(parsed);
        CHECK(std::string(oscpm::toString(parsed.error())) == corpusCase.errorName);
        CHECK_FALSE(matched);
        return;
    }

    REQUIRE(parsed);
    CHECK(parsed.pattern().text() == corpusCase.pattern);
    const bool matchedByValue = parsed.pattern().matches(corpusCase.address);
    const oscpm::Address::ParseResult address = oscpm::Address::parse(corpusCase.address);

    if (corpusCase.expectation == Expectation::MalformedAddress)
    {
        REQUIRE_FALSE(address);
        CHECK(std::string(oscpm::toString(address.error())) == corpusCase.errorName);
        CHECK(matched == corpusCase.matchesBytewise);
        CHECK(matchedByValue == corpusCase.matchesBytewise);
        return;
    }

    REQUIRE(address);
    CHECK(address.address().text() == corpusCase.address);
    oscpm::AddressSpace<int, false> space;
    CHECK(space.add(address.address(), 0));
    CHECK(space.size() == 1);
    CHECK(matched == (corpusCase.expectation == Expectation::Match));
    CHECK(matchedByValue == (corpusCase.expectation == Expectation::Match));
}

bool contains(const std::vector<CorpusCase>& cases, Expectation expectation)
{
    for (const CorpusCase& corpusCase : cases)
    {
        if (corpusCase.expectation == expectation)
        {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("the corpus exercises every kind of expectation")
{
    const std::vector<CorpusCase> cases = oscpm_test::loadCorpus(OSCPM_CORPUS_PATH);
    CHECK(contains(cases, Expectation::Match));
    CHECK(contains(cases, Expectation::NoMatch));
    CHECK(contains(cases, Expectation::MalformedPattern));
    CHECK(contains(cases, Expectation::MalformedAddress));
}

TEST_CASE("every corpus case holds through parse, matches, match and add")
{
    for (const CorpusCase& corpusCase : oscpm_test::loadCorpus(OSCPM_CORPUS_PATH))
    {
        checkCase(corpusCase);
    }
}
