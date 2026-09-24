/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "corpus.h"

#include <doctest/doctest.h>

#include <fstream>
#include <ios>
#include <regex>
#include <sstream>
#include <utility>

namespace oscpm_test
{

namespace
{

    const std::regex kCaseLine(R"re(^[ \t]*"([^"]*)"[ \t]*"([^"]*)"[ \t]*(.*?)[ \t]*$)re");

    bool parseExpectation(const std::string& words, CorpusCase& corpusCase)
    {
        std::istringstream stream(words);
        std::string kind;
        std::string outcome;
        stream >> kind;
        if (kind == "match" || kind == "nomatch")
        {
            corpusCase.expectation = kind == "match" ? Expectation::Match : Expectation::NoMatch;
            corpusCase.matchesBytewise = kind == "match";
        }
        else if (kind == "malformed-pattern")
        {
            corpusCase.expectation = Expectation::MalformedPattern;
            stream >> corpusCase.errorName >> corpusCase.offset;
        }
        else if (kind == "malformed-address")
        {
            corpusCase.expectation = Expectation::MalformedAddress;
            stream >> corpusCase.errorName >> corpusCase.offset >> outcome;
            corpusCase.matchesBytewise = outcome == "match";
            if (outcome != "match" && outcome != "nomatch")
            {
                return false;
            }
        }
        else
        {
            return false;
        }
        std::string trailing;
        return !stream.fail() && !(stream >> trailing);
    }

}

std::vector<CorpusCase> loadCorpus(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        FAIL("cannot open corpus at " << path);
    }

    std::vector<CorpusCase> cases;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#')
        {
            continue;
        }
        std::smatch fields;
        CorpusCase corpusCase;
        if (!std::regex_match(line, fields, kCaseLine) || !parseExpectation(fields[3], corpusCase))
        {
            FAIL("corpus line " << lineNumber << " is not a case: " << line);
        }
        corpusCase.line = lineNumber;
        corpusCase.text = line;
        corpusCase.pattern = fields[1];
        corpusCase.address = fields[2];
        cases.push_back(std::move(corpusCase));
    }
    if (cases.empty())
    {
        FAIL("corpus at " << path << " contains no cases");
    }
    return cases;
}

}
