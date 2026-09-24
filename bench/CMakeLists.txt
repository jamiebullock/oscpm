# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
set(BENCHMARK_ENABLE_INSTALL OFF)
set(BENCHMARK_ENABLE_WERROR OFF)

# SYSTEM keeps Google Benchmark's headers out of the warnings the benchmarks
# are built with.
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG 192ef10025eb2c4cdd392bc502f0c852196baa48 # v1.9.5
    SYSTEM
)
FetchContent_MakeAvailable(benchmark)

set(OSCPM_BENCH_INCLUDE_DIR "" CACHE PATH
    "oscpm headers to benchmark in place of this tree's, for comparing two revisions")

add_executable(oscpm_bench
    dispatch_bench.cpp
    main.cpp
    match_bench.cpp
    workload.cpp
)

if(OSCPM_BENCH_INCLUDE_DIR)
    target_include_directories(oscpm_bench PRIVATE ${OSCPM_BENCH_INCLUDE_DIR})
else()
    target_link_libraries(oscpm_bench PRIVATE oscpm::oscpm)
endif()

target_link_libraries(oscpm_bench PRIVATE benchmark::benchmark)

target_compile_definitions(oscpm_bench PRIVATE
    OSCPM_BENCH_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

target_compile_options(oscpm_bench PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

add_test(NAME oscpm_bench_smoke COMMAND oscpm_bench --benchmark_min_time=1x)
set_tests_properties(oscpm_bench_smoke PROPERTIES LABELS bench)
