# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# Checks the seed corpus that seed_corpus.cmake generates from
# corpus/matching.txt. Run as a ctest:
#
#   cmake -DCORPUS=<matching.txt> -DSEEDS=<seed directory> -DPART_LIMIT=<bytes> -P check_seeds.cmake
#
# One seed per case, holding the pattern, a newline, and the address, with
# the corpus's double quotes stripped, plus one seed whose pattern part is
# one byte longer than PART_LIMIT. The expected contents below are written
# out by hand from the corpus, not derived from it.
#
# Seed contents are never put in a CMake list: an unbalanced '[' in a list
# element folds the elements after it into that one.

if(NOT CORPUS OR NOT SEEDS OR NOT PART_LIMIT)
    message(FATAL_ERROR
        "usage: cmake -DCORPUS=<file> -DSEEDS=<dir> -DPART_LIMIT=<bytes> -P check_seeds.cmake"
    )
endif()
if(NOT EXISTS "${SEEDS}")
    message(FATAL_ERROR "seed directory ${SEEDS} does not exist")
endif()

file(GLOB seedFiles LIST_DIRECTORIES false "${SEEDS}/*")
list(LENGTH seedFiles seedCount)

# An independent count of the cases: every case line, and only a case line,
# opens with a double quote. A newline is prepended so that every line, the
# first included, is found by the newline before it.
file(READ "${CORPUS}" corpus)
string(REGEX MATCHALL "\n[ \t]*\"" caseOpenings "\n${corpus}")
list(LENGTH caseOpenings caseCount)
math(EXPR wantCount "${caseCount} + 1")
if(NOT seedCount EQUAL wantCount)
    message(FATAL_ERROR "${seedCount} seeds for ${caseCount} corpus cases plus the overlong part")
endif()

# Every seed, each wrapped in double quotes, which no seed may contain, so
# that a quoted search string matches one whole seed and nothing else.
set(allSeedsQuoted "")
set(overlong 0)
foreach(seedFile IN LISTS seedFiles)
    file(READ "${seedFile}" seed)
    if(seed MATCHES "\"")
        message(FATAL_ERROR "${seedFile} still carries a double quote: ${seed}")
    endif()
    string(APPEND allSeedsQuoted "\"${seed}\"")
    # A pattern of '/' and PART_LIMIT + 1 'a's, then the newline and the
    # address "/a": PART_LIMIT + 5 bytes in all.
    string(LENGTH "${seed}" seedLength)
    math(EXPR overlongLength "${PART_LIMIT} + 5")
    if(seedLength EQUAL overlongLength AND seed MATCHES "^/a+\n/a$")
        math(EXPR overlong "${overlong} + 1")
    endif()
endforeach()
if(NOT overlong EQUAL 1)
    message(FATAL_ERROR "expected exactly one overlong-part seed, found ${overlong}")
endif()

# A plain case; an empty pattern; a space inside a pattern, which the corpus
# quotes for exactly this reason; a two-byte UTF-8 sequence; a case with a
# '#', which must not be mistaken for a comment; and an unterminated '[' in
# the pattern.
foreach(want IN ITEMS
    "/a*b\n/aXbYb"
    "\n/a"
    "/a b\n/a b"
    "/??\n/é"
    "/a\n/a#"
    "/x/[a/b]\n/x/a"
)
    string(FIND "${allSeedsQuoted}" "\"${want}\"" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "no seed holds: ${want}")
    endif()
endforeach()

message(STATUS "${seedCount} seeds match the corpus")
