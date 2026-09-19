#pragma once

// Loads corpus/matching.txt so that both the matcher and the Address Space
// can be checked against the same cases. The file format is documented at
// the head of the corpus.

#include <cstddef>
#include <string>
#include <vector>

namespace oscpm_test {

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

// Parses every case in the corpus at `path`. Fails the running test on an
// unreadable file, an unparseable line, or an empty corpus.
std::vector<Case> loadCorpus(const char* path);

} // namespace oscpm_test
