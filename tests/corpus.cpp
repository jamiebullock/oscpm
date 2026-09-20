/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "corpus.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <ios>
#include <sstream>
#include <utility>

namespace oscpm_test
{

namespace
{

    std::string quotedField(const std::string& line, std::size_t& position, int lineNumber)
    {
        const std::size_t open = line.find('"', position);
        if (open == std::string::npos)
        {
            FAIL("corpus line " << lineNumber << ": expected a quoted field: " << line);
        }
        const std::size_t close = line.find('"', open + 1);
        if (close == std::string::npos)
        {
            FAIL("corpus line " << lineNumber << ": unterminated quoted field: " << line);
        }
        position = close + 1;
        return line.substr(open + 1, close - open - 1);
    }

    bool matchWord(const std::string& word, int lineNumber, const std::string& line)
    {
        if (word == "match")
        {
            return true;
        }
        if (word == "nomatch")
        {
            return false;
        }
        FAIL("corpus line " << lineNumber << ": expected match or nomatch, got '" << word << "': " << line);
        return false;
    }

    CorpusCase parseCase(const std::string& line, int lineNumber, std::size_t first)
    {
        CorpusCase corpusCase;
        corpusCase.line = lineNumber;
        corpusCase.text = line;
        std::size_t position = first;
        corpusCase.pattern = quotedField(line, position, lineNumber);
        corpusCase.address = quotedField(line, position, lineNumber);

        std::istringstream rest(line.substr(position));
        std::string expectation;
        rest >> expectation;
        if (expectation == "match" || expectation == "nomatch")
        {
            corpusCase.matchesBytewise = matchWord(expectation, lineNumber, line);
            corpusCase.expectation = corpusCase.matchesBytewise ? Expectation::Match : Expectation::NoMatch;
        }
        else if (expectation == "malformed-pattern")
        {
            corpusCase.expectation = Expectation::MalformedPattern;
            if (!(rest >> corpusCase.errorName >> corpusCase.offset))
            {
                FAIL("corpus line " << lineNumber << ": malformed-pattern needs <Error> <offset>: " << line);
            }
        }
        else if (expectation == "malformed-address")
        {
            corpusCase.expectation = Expectation::MalformedAddress;
            std::string outcome;
            if (!(rest >> corpusCase.errorName >> corpusCase.offset >> outcome))
            {
                FAIL("corpus line " << lineNumber << ": malformed-address needs <Error> <offset> <match|nomatch>: " << line);
            }
            corpusCase.matchesBytewise = matchWord(outcome, lineNumber, line);
        }
        else
        {
            FAIL("corpus line " << lineNumber << ": unknown expectation '" << expectation << "': " << line);
        }
        std::string trailing;
        if (rest >> trailing)
        {
            FAIL("corpus line " << lineNumber << ": unexpected trailing text '" << trailing << "': " << line);
        }
        return corpusCase;
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
        cases.push_back(parseCase(line, lineNumber, first));
    }
    if (cases.empty())
    {
        FAIL("corpus at " << path << " contains no cases");
    }
    return cases;
}

}
