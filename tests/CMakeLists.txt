# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

include(FetchContent)

# Catch2 defaults to C++14, which leaves out its std::string_view support.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 317ac1ed4c0bb6e6b91eafc817e05c488feffcb3 # v3.16.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(oscpm_flat_tests
    flat_tests.cpp
)

target_link_libraries(oscpm_flat_tests PRIVATE
    oscpm_flat::oscpm_flat
    Catch2::Catch2WithMain
)

target_compile_options(oscpm_flat_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

include(Catch)
catch_discover_tests(oscpm_flat_tests)
