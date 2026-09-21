# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# Writes one seed per corpus case, holding the pattern, a newline and the
# address, plus one seed with a part one byte beyond PART_LIMIT:
#
#   cmake -DCORPUS=<matching.txt> -DSEEDS=<output directory> -DPART_LIMIT=<bytes> -P seed_corpus.cmake
#
# The corpus is walked with string(FIND), because an unbalanced '[' in a
# CMake list element folds the elements after it into that one.

if(NOT CORPUS OR NOT SEEDS OR NOT PART_LIMIT)
    message(FATAL_ERROR
        "usage: cmake -DCORPUS=<file> -DSEEDS=<dir> -DPART_LIMIT=<bytes> -P seed_corpus.cmake"
    )
endif()

file(REMOVE_RECURSE "${SEEDS}")
file(MAKE_DIRECTORY "${SEEDS}")

file(READ "${CORPUS}" corpus)
set(rest "${corpus}")

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

# Every case line, and only a case line, opens with a double quote.
string(REGEX MATCHALL "\n[ \t]*\"" caseOpenings "\n${corpus}")
list(LENGTH caseOpenings caseCount)
if(caseCount EQUAL 0 OR NOT seedCount EQUAL caseCount)
    message(FATAL_ERROR "${seedCount} seeds written for ${caseCount} cases in ${CORPUS}")
endif()

# libFuzzer grows inputs slowly from small seeds and in a minute never
# reaches the length check on its own.
set(longPart "")
foreach(byte RANGE ${PART_LIMIT})
    string(APPEND longPart "a")
endforeach()
file(WRITE "${SEEDS}/part-too-long" "/${longPart}\n/a")
math(EXPR seedCount "${seedCount} + 1")

message(STATUS "wrote ${seedCount} seeds to ${SEEDS}")
