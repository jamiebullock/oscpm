// Loads corpus/matching.txt and checks every case against the matcher.
// The file format is documented at the head of the corpus.

#include <oscpm/pattern.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class Expectation { Match, NoMatch, MalformedPattern, MalformedAddress };

struct Case {
    int line = 0;
    std::string text;
    std::string pattern;
    std::string address;
    Expectation expectation = Expectation::Match;
    std::string kind;
    std::size_t offset = 0;
};

// Extracts the next double-quoted field starting at `pos`, leaving `pos`
// just past its closing quote.
std::string quotedField(const std::string& line, std::size_t& pos, int lineNumber)
{
    const std::size_t open = line.find('"', pos);
    if (open == std::string::npos) {
        FAIL("corpus line " << lineNumber << ": expected a quoted field: " << line);
    }
    const std::size_t close = line.find('"', open + 1);
    if (close == std::string::npos) {
        FAIL("corpus line " << lineNumber << ": unterminated quoted field: " << line);
    }
    pos = close + 1;
    return line.substr(open + 1, close - open - 1);
}

std::vector<Case> loadCorpus(const char* path)
{
    std::ifstream in(path);
    if (!in) {
        FAIL("cannot open corpus at " << path);
    }

    std::vector<Case> cases;
    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }

        Case c;
        c.line = lineNumber;
        c.text = line;
        std::size_t pos = first;
        c.pattern = quotedField(line, pos, lineNumber);
        c.address = quotedField(line, pos, lineNumber);

        std::istringstream rest(line.substr(pos));
        std::string verdict;
        rest >> verdict;
        if (verdict == "match") {
            c.expectation = Expectation::Match;
        } else if (verdict == "nomatch") {
            c.expectation = Expectation::NoMatch;
        } else if (verdict == "malformed-pattern" || verdict == "malformed-address") {
            c.expectation = verdict == "malformed-pattern" ? Expectation::MalformedPattern
                                                           : Expectation::MalformedAddress;
            if (!(rest >> c.kind >> c.offset)) {
                FAIL("corpus line " << lineNumber << ": " << verdict
                                    << " needs <ErrorKind> <offset>: " << line);
            }
        } else {
            FAIL("corpus line " << lineNumber << ": unknown expectation '" << verdict
                                << "': " << line);
        }
        std::string trailing;
        if (rest >> trailing) {
            FAIL("corpus line " << lineNumber << ": unexpected trailing text '" << trailing
                                << "': " << line);
        }
        cases.push_back(std::move(c));
    }
    if (cases.empty()) {
        FAIL("corpus at " << path << " contains no cases");
    }
    return cases;
}

void checkCase(const Case& c)
{
    INFO("corpus line " << c.line << ": " << c.text);

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(c.pattern);
    const oscpm::MatchResult convenience = oscpm::match(c.pattern, c.address);

    if (c.expectation == Expectation::MalformedPattern) {
        REQUIRE_FALSE(parsed.ok());
        CHECK(oscpm::name(parsed.error().kind) == c.kind);
        CHECK(parsed.error().offset == c.offset);
        CHECK(convenience == oscpm::MatchResult::Malformed);
        return;
    }

    REQUIRE(parsed.ok());
    const std::optional<oscpm::Error> addressError = oscpm::validateAddress(c.address);

    if (c.expectation == Expectation::MalformedAddress) {
        REQUIRE(addressError.has_value());
        CHECK(oscpm::name(addressError->kind) == c.kind);
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
    for (const Case& c : loadCorpus(OSCPM_CORPUS_PATH)) {
        checkCase(c);
    }
}
