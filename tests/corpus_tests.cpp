/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "corpus.h"

#include <oscpm/oscpm.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

namespace
{

using oscpm_test::CorpusCase;
using oscpm_test::Expectation;

std::string errorName(const std::optional<oscpm::ParseError>& error)
{
    return error.has_value() ? oscpm::toString(error->kind) : "";
}

void checkCase(const CorpusCase& corpusCase)
{
    INFO("corpus line " << corpusCase.line << ": " << corpusCase.text);

    const std::optional<oscpm::ParseError> patternError = oscpm::validatePattern(corpusCase.pattern);
    const bool matched = oscpm::match(corpusCase.pattern, corpusCase.address);

    if (corpusCase.expectation == Expectation::MalformedPattern)
    {
        REQUIRE(patternError.has_value());
        CHECK(errorName(patternError) == corpusCase.errorName);
        CHECK(patternError->offset == corpusCase.offset);
        CHECK_FALSE(matched);
        return;
    }

    REQUIRE_FALSE(patternError.has_value());
    const std::optional<oscpm::ParseError> addressError = oscpm::validateAddress(corpusCase.address);

    if (corpusCase.expectation == Expectation::MalformedAddress)
    {
        REQUIRE(addressError.has_value());
        CHECK(errorName(addressError) == corpusCase.errorName);
        CHECK(addressError->offset == corpusCase.offset);
        CHECK(matched == corpusCase.matchesBytewise);
        return;
    }

    CHECK_FALSE(addressError.has_value());
    CHECK(matched == (corpusCase.expectation == Expectation::Match));
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

TEST_CASE("every corpus case holds through match and both validators")
{
    for (const CorpusCase& corpusCase : oscpm_test::loadCorpus(OSCPM_CORPUS_PATH))
    {
        checkCase(corpusCase);
    }
}
