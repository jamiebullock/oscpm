# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151 # v2.5.3
)
FetchContent_MakeAvailable(doctest)

add_executable(oscpm_tests
    address_space_tests.cpp
    allocation_counter.cpp
    corpus.cpp
    corpus_tests.cpp
    error_tests.cpp
    match_tests.cpp
    pattern_tests.cpp
)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt"
)

target_link_libraries(oscpm_tests PRIVATE
    oscpm::oscpm
    doctest::doctest_with_main
)

target_compile_options(oscpm_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

if(OSCPM_SANITIZE)
    target_compile_options(oscpm_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(oscpm_tests PRIVATE -fsanitize=address,undefined)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(oscpm_tests)

add_test(NAME "oscpm builds as a subproject of a project with a CMake module of the same name"
    COMMAND ${CMAKE_CTEST_COMMAND} --build-and-test
        ${CMAKE_CURRENT_SOURCE_DIR}/consumer ${CMAKE_CURRENT_BINARY_DIR}/consumer
        --build-generator ${CMAKE_GENERATOR}
        --build-config $<CONFIG>
        --build-options -DOSCPM_SOURCE_DIR=${PROJECT_SOURCE_DIR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        --test-command oscpm_consumer)

add_executable(oscpm_visitor_rule visitor_rule.cpp)
target_link_libraries(oscpm_visitor_rule PRIVATE oscpm::oscpm)
target_compile_options(oscpm_visitor_rule PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)
add_test(NAME "a visitor that removes a method trips an assertion in a build without NDEBUG" COMMAND oscpm_visitor_rule)
set_tests_properties("a visitor that removes a method trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must not add or remove methods|assertions are compiled out")
add_test(NAME "a visitor that assigns to the space trips an assertion in a build without NDEBUG" COMMAND oscpm_visitor_rule assign)
set_tests_properties("a visitor that assigns to the space trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must not be assigned to during a visit|assertions are compiled out")
