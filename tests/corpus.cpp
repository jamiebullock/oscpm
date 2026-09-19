#include "corpus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace oscpm_test {

namespace {

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

} // namespace

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
        std::string expectation;
        rest >> expectation;
        if (expectation == "match") {
            c.expectation = Expectation::Match;
        } else if (expectation == "nomatch") {
            c.expectation = Expectation::NoMatch;
        } else if (expectation == "malformed-pattern" || expectation == "malformed-address") {
            c.expectation = expectation == "malformed-pattern" ? Expectation::MalformedPattern
                                                               : Expectation::MalformedAddress;
            if (!(rest >> c.kind >> c.offset)) {
                FAIL("corpus line " << lineNumber << ": " << expectation
                                    << " needs <ErrorKind> <offset>: " << line);
            }
        } else {
            FAIL("corpus line " << lineNumber << ": unknown expectation '" << expectation
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

} // namespace oscpm_test
