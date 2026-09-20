# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 317ac1ed4c0bb6e6b91eafc817e05c488feffcb3 # v3.16.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(oscpm_tests
    corpus.cpp
    corpus_tests.cpp
    match_tests.cpp
    validation_tests.cpp
)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt"
)

target_link_libraries(oscpm_tests PRIVATE
    oscpm::oscpm
    Catch2::Catch2WithMain
)

target_compile_options(oscpm_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

include(Catch)
catch_discover_tests(oscpm_tests)
