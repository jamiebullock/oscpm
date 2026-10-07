# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

set(CMAKE_FOLDER tests)

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
    address_tests.cpp
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

if(OSCPM_MULL_PLUGIN)
    set(OSCPM_MULL_RUNNER mull-runner CACHE STRING "Mull runner of the same LLVM version as OSCPM_MULL_PLUGIN")
    set(OSCPM_MUTATION_THRESHOLD 89 CACHE STRING "Lowest mutation score, out of 100, the mutation target accepts")
    target_compile_options(oscpm_tests PRIVATE -fpass-plugin=${OSCPM_MULL_PLUGIN} -g -grecord-command-line)
    # Makes libstdc++ fail on an out-of-range index, so a mutant that reads one element past a view or an array is killed.
    target_compile_definitions(oscpm_tests PRIVATE _GLIBCXX_ASSERTIONS)
    find_program(OSCPM_MULL_RUNNER_PATH NAMES ${OSCPM_MULL_RUNNER} REQUIRED)
    # Mull's instrumentation calls getenv on every call of a mutated function, so the tests slow down with the size of the environment.
    add_custom_target(mutation
        COMMAND env -i ${OSCPM_MULL_RUNNER_PATH}
            --reporters IDE --reporters Elements
            --report-dir ${CMAKE_BINARY_DIR}/reports --report-name mutation
            --timeout 10000
            --mutation-score-threshold ${OSCPM_MUTATION_THRESHOLD}
            $<TARGET_FILE:oscpm_tests>
        DEPENDS oscpm_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        VERBATIM
    )
endif()

set_target_properties(oscpm_tests PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

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
add_test(NAME "a visitor that removes an address trips an assertion in a build without NDEBUG" COMMAND oscpm_visitor_rule)
set_tests_properties("a visitor that removes an address trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must not add to or remove from|assertions are compiled out")

set(OSCPM_VISITOR_CASES
    "visit with a pattern on a space|visit needs a visitor callable as visitor.std::string_view address, T& value"
    "visit with a pattern on a const space|visit needs a visitor callable as visitor.std::string_view address, const T& value"
    "visit without a pattern on a space|visit needs a visitor callable as visitor.std::string_view address, T& value"
    "visit without a pattern on a const space|visit needs a visitor callable as visitor.std::string_view address, const T& value"
)
set(caseNumber 0)
foreach(visitorCase IN LISTS OSCPM_VISITOR_CASES)
    math(EXPR caseNumber "${caseNumber} + 1")
    string(REPLACE "|" ";" visitorCase "${visitorCase}")
    list(GET visitorCase 0 caseName)
    list(GET visitorCase 1 expectedMessage)
    set(target oscpm_visitor_signature_${caseNumber})
    add_executable(${target} EXCLUDE_FROM_ALL visitor_signature.cpp)
    target_link_libraries(${target} PRIVATE oscpm::oscpm)
    target_compile_definitions(${target} PRIVATE OSCPM_VISITOR_CASE=${caseNumber})
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    set(testName "${caseName} fails to compile with a visitor of the wrong signature")
    add_test(NAME "${testName}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target} --config $<CONFIG>)
    set_tests_properties("${testName}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${expectedMessage}"
        RESOURCE_LOCK oscpm_build_tree)
endforeach()

add_executable(oscpm_address_rule address_rule.cpp)
target_link_libraries(oscpm_address_rule PRIVATE oscpm::oscpm)
target_compile_options(oscpm_address_rule PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)
add_test(NAME "an address whose bytes changed after parsing trips an assertion in a build without NDEBUG" COMMAND oscpm_address_rule)
set_tests_properties("an address whose bytes changed after parsing trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must stay unchanged after it is parsed|assertions are compiled out")
