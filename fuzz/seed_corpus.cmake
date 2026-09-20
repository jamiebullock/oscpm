# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# Generates the fuzz seed corpus from corpus/matching.txt:
#
#   cmake -DCORPUS=<matching.txt> -DSEEDS=<output directory> -DPART_LIMIT=<bytes> -P seed_corpus.cmake
#
# Every case becomes one seed named after its corpus line, holding the
# pattern, a newline, and the address: the input format pattern_fuzz.cpp
# reads. The expectation is left to the conformance tests. The output
# directory is recreated from scratch so that no seed outlives its case.
#
# The corpus is walked with string(FIND), not as a CMake list: an unbalanced
# '[' in a list element, which many cases contain, makes CMake fold the
# elements after it into that one.

if(NOT CORPUS OR NOT SEEDS OR NOT PART_LIMIT)
    message(FATAL_ERROR
        "usage: cmake -DCORPUS=<file> -DSEEDS=<dir> -DPART_LIMIT=<bytes> -P seed_corpus.cmake"
    )
endif()

file(REMOVE_RECURSE "${SEEDS}")
file(MAKE_DIRECTORY "${SEEDS}")

file(READ "${CORPUS}" rest)

set(lineNumber 0)
set(seedCount 0)
while(NOT rest STREQUAL "")
    string(FIND "${rest}" "\n" newline)
    if(newline EQUAL -1)
        set(line "${rest}")
        set(rest "")
    else()
        string(SUBSTRING "${rest}" 0 ${newline} line)
        math(EXPR next "${newline} + 1")
        string(SUBSTRING "${rest}" ${next} -1 rest)
    endif()
    math(EXPR lineNumber "${lineNumber} + 1")
    string(REGEX REPLACE "\r$" "" line "${line}")
    if(line MATCHES "^[ \t]*\"([^\"]*)\"[ \t]*\"([^\"]*)\"")
        file(WRITE "${SEEDS}/line-${lineNumber}" "${CMAKE_MATCH_1}\n${CMAKE_MATCH_2}")
        math(EXPR seedCount "${seedCount} + 1")
    endif()
endwhile()

if(seedCount EQUAL 0)
    message(FATAL_ERROR "no cases found in ${CORPUS}")
endif()

# One seed the corpus does not carry: a part one byte beyond PART_LIMIT.
# libFuzzer grows inputs slowly from small seeds and in a minute never
# reaches the length check on its own.
set(longPart "")
foreach(byte RANGE ${PART_LIMIT})
    string(APPEND longPart "a")
endforeach()
file(WRITE "${SEEDS}/part-too-long" "/${longPart}\n/a")
math(EXPR seedCount "${seedCount} + 1")

message(STATUS "wrote ${seedCount} seeds to ${SEEDS}")
