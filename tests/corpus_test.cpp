// Checks every case in corpus/matching.txt against the standalone matcher.

#include <oscpm/pattern.hpp>

#include "corpus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace {

using oscpm_test::Case;
using oscpm_test::Expectation;

void checkCase(const Case& c)
{
    INFO("corpus line " << c.line << ": " << c.text);

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(c.pattern);
    const oscpm::MatchResult convenience = oscpm::match(c.pattern, c.address);

    if (c.expectation == Expectation::MalformedPattern) {
        REQUIRE_FALSE(parsed.ok());
        CHECK(oscpm::toString(parsed.error().kind) == c.kind);
        CHECK(parsed.error().offset == c.offset);
        CHECK(convenience == oscpm::MatchResult::Malformed);
        return;
    }

    REQUIRE(parsed.ok());
    const std::optional<oscpm::Error> addressError = oscpm::validateAddress(c.address);

    if (c.expectation == Expectation::MalformedAddress) {
        REQUIRE(addressError.has_value());
        CHECK(oscpm::toString(addressError->kind) == c.kind);
        CHECK(addressError->offset == c.offset);
        CHECK(parsed.pattern().matches(c.address) == oscpm::MatchResult::Malformed);
        CHECK(convenience == oscpm::MatchResult::Malformed);
        return;
    }

    CHECK_FALSE(addressError.has_value());
    const oscpm::MatchResult expected = c.expectation == Expectation::Match
                                            ? oscpm::MatchResult::Match
                                            : oscpm::MatchResult::NoMatch;
    CHECK(parsed.pattern().matches(c.address) == expected);
    CHECK(convenience == expected);
}

} // namespace

TEST_CASE("conformance corpus", "[corpus]")
{
    for (const Case& c : oscpm_test::loadCorpus(OSCPM_CORPUS_PATH)) {
        checkCase(c);
    }
}
