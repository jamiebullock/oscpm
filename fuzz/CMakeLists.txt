# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# The seed corpus is generated at build time from corpus/matching.txt, so it
# tracks the conformance corpus without a checked-in copy.
set(OSCPM_FUZZ_SEEDS_DIR ${CMAKE_CURRENT_BINARY_DIR}/seeds)
set(OSCPM_FUZZ_SEEDS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/seeds.stamp)

# Must equal oscpm::kMaxAddressPartLength; the scripts write and check a seed
# whose part is one byte longer.
set(OSCPM_FUZZ_PART_LIMIT 4095)

set(OSCPM_FUZZ_SCRIPT_ARGS
    -DCORPUS=${PROJECT_SOURCE_DIR}/corpus/matching.txt
    -DSEEDS=${OSCPM_FUZZ_SEEDS_DIR}
    -DPART_LIMIT=${OSCPM_FUZZ_PART_LIMIT}
)

add_custom_command(
    OUTPUT ${OSCPM_FUZZ_SEEDS_STAMP}
    COMMAND ${CMAKE_COMMAND} ${OSCPM_FUZZ_SCRIPT_ARGS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/seed_corpus.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${OSCPM_FUZZ_SEEDS_STAMP}
    DEPENDS
        ${PROJECT_SOURCE_DIR}/corpus/matching.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/seed_corpus.cmake
    COMMENT "Generating the fuzz seed corpus from corpus/matching.txt"
    VERBATIM
)

add_custom_target(oscpm_fuzz_seeds ALL DEPENDS ${OSCPM_FUZZ_SEEDS_STAMP})

add_test(NAME oscpm_fuzz_seeds
    COMMAND ${CMAKE_COMMAND} ${OSCPM_FUZZ_SCRIPT_ARGS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_seeds.cmake
)

# libFuzzer ships with LLVM clang but not with Apple clang, GCC or MSVC, so
# the target is enabled by a link probe of the exact flags it needs.
set(OSCPM_FUZZ_SANITIZE -fsanitize=fuzzer,address,undefined)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "${OSCPM_FUZZ_SANITIZE}")
set(CMAKE_REQUIRED_LINK_OPTIONS "${OSCPM_FUZZ_SANITIZE}")
check_cxx_source_compiles(
    "extern \"C\" int LLVMFuzzerTestOneInput(const unsigned char*, unsigned long) { return 0; }"
    OSCPM_HAVE_LIBFUZZER
)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LINK_OPTIONS)

if(NOT OSCPM_HAVE_LIBFUZZER)
    message(WARNING
        "OSCPM_BUILD_FUZZERS is ON but ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
        "cannot link libFuzzer with the sanitizers, so the fuzz target is skipped. Use an LLVM "
        "clang; on macOS, -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++."
    )
    return()
endif()

# libFuzzer writes the inputs it discovers here, so a run can be resumed and
# a finding is kept; the seeds are read from their own directory and never
# modified.
set(OSCPM_FUZZ_CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpus)
file(MAKE_DIRECTORY ${OSCPM_FUZZ_CORPUS_DIR})

add_executable(oscpm_pattern_fuzz pattern_fuzz.cpp)
target_link_libraries(oscpm_pattern_fuzz PRIVATE oscpm::oscpm)
target_compile_options(oscpm_pattern_fuzz PRIVATE
    ${OSCPM_FUZZ_SANITIZE}
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer
    -g
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror
)
target_link_options(oscpm_pattern_fuzz PRIVATE ${OSCPM_FUZZ_SANITIZE})
add_dependencies(oscpm_pattern_fuzz oscpm_fuzz_seeds)

# Every seed once, under the sanitizers, with no mutation.
add_test(NAME oscpm_pattern_fuzz_seeds
    COMMAND oscpm_pattern_fuzz -runs=0 ${OSCPM_FUZZ_SEEDS_DIR}
)
