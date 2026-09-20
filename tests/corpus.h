/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace oscpm_test
{

enum class Expectation
{
    Match,
    NoMatch,
    MalformedPattern,
    MalformedAddress
};

struct CorpusCase
{
    int line = 0;
    std::string text;
    std::string pattern;
    std::string address;
    Expectation expectation = Expectation::Match;
    std::string errorName;
    std::size_t offset = 0;
    bool matchesBytewise = false;
};

/// Every case in the corpus file at `path`, read as bytes; fails the running
/// test on an unreadable file, an unparseable line or an empty corpus.
std::vector<CorpusCase> loadCorpus(const char* path);

}
