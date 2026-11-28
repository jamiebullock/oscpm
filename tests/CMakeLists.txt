# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

set(CMAKE_FOLDER tests)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DOCTEST_NO_INSTALL ON)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151 # v2.5.3
)
FetchContent_MakeAvailable(doctest)

function(oscpm_add_test_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE oscpm::oscpm)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
    )
endfunction()

oscpm_add_test_executable(oscpm_tests
    address_space_model_tests.cpp
    address_space_tests.cpp
    allocation_counter.cpp
    corpus.cpp
    corpus_tests.cpp
    address_tests.cpp
    error_tests.cpp
    expected_tests.cpp
    match_tests.cpp
    pattern_tests.cpp
    word_tests.cpp
)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt"
)

target_link_libraries(oscpm_tests PRIVATE doctest::doctest_with_main)

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

# An absolute install directory is used as it stands, whatever the prefix, so
# an install to one would write outside the build tree.
if(NOT IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}" AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    add_test(NAME "an install contains oscpm's headers and package files and nothing else"
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed
            -DCONFIG=$<CONFIG>
            -DINCLUDE_DIR=${CMAKE_INSTALL_INCLUDEDIR}
            -DLIBRARY_DIR=${CMAKE_INSTALL_LIBDIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_files.cmake)
    set_tests_properties("an install contains oscpm's headers and package files and nothing else" PROPERTIES
        FIXTURES_SETUP oscpm_installed)

    add_test(NAME "a project that finds an installed oscpm with find_package builds against it"
        COMMAND ${CMAKE_CTEST_COMMAND} --build-and-test
            ${CMAKE_CURRENT_SOURCE_DIR}/consumer ${CMAKE_CURRENT_BINARY_DIR}/consumer_of_install
            --build-generator ${CMAKE_GENERATOR}
            --build-config $<CONFIG>
            --build-options -Doscpm_DIR=${CMAKE_CURRENT_BINARY_DIR}/installed/${CMAKE_INSTALL_LIBDIR}/cmake/oscpm -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            --test-command oscpm_consumer)
    set_tests_properties("a project that finds an installed oscpm with find_package builds against it" PROPERTIES
        FIXTURES_REQUIRED oscpm_installed)
endif()

oscpm_add_test_executable(oscpm_rules rules.cpp)
add_test(NAME "a visitor that removes an address trips an assertion in a build without NDEBUG" COMMAND oscpm_rules remove-from-visitor)
set_tests_properties("a visitor that removes an address trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must not add to or remove from")

function(oscpm_add_compile_failure target source definition testName expectedMessage)
    add_executable(${target} EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${target} PRIVATE oscpm::oscpm)
    target_compile_definitions(${target} PRIVATE ${definition})
    target_compile_options(${target} PRIVATE ${ARGN})
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    add_test(NAME "${testName}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target} --config $<CONFIG>)
    set_tests_properties("${testName}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${expectedMessage}"
        RESOURCE_LOCK oscpm_build_tree)
endfunction()

# Each argument after the definition is a test name, or EXPECT <message> or
# COMPILE_OPTION <option>, which applies to the test names after it. The Nth
# test builds <targetPrefix>_N from the source with <definition>=N.
function(oscpm_add_compile_failures targetPrefix source definition)
    set(caseNumber 0)
    set(expectedMessage "")
    set(compileOptions "")
    set(keyword "")
    foreach(argument IN LISTS ARGN)
        if(keyword STREQUAL "EXPECT")
            set(expectedMessage "${argument}")
            set(keyword "")
        elseif(keyword STREQUAL "COMPILE_OPTION")
            list(APPEND compileOptions "${argument}")
            set(keyword "")
        elseif(argument STREQUAL "EXPECT" OR argument STREQUAL "COMPILE_OPTION")
            set(keyword "${argument}")
        elseif(expectedMessage STREQUAL "")
            message(FATAL_ERROR "No EXPECT message precedes \"${argument}\"")
        else()
            math(EXPR caseNumber "${caseNumber} + 1")
            oscpm_add_compile_failure(${targetPrefix}_${caseNumber} ${source} ${definition}=${caseNumber}
                "${argument}" "${expectedMessage}" ${compileOptions})
        endif()
    endforeach()
endfunction()

oscpm_add_compile_failures(oscpm_visitor_signature visitor_signature.cpp OSCPM_VISITOR_CASE
    EXPECT "visit needs a visitor callable as visitor.std::string_view address, T& value"
    "visit with a pattern on a space fails to compile with a visitor of the wrong signature"
    EXPECT "visit needs a visitor callable as visitor.std::string_view address, const T& value"
    "visit with a pattern on a const space fails to compile with a visitor of the wrong signature"
    EXPECT "visit needs a visitor callable as visitor.std::string_view address, T& value"
    "visit without a pattern on a space fails to compile with a visitor of the wrong signature"
    EXPECT "visit needs a visitor callable as visitor.std::string_view address, const T& value"
    "visit without a pattern on a const space fails to compile with a visitor of the wrong signature"
)

oscpm_add_compile_failures(oscpm_dispatch_signature dispatch_signature.cpp OSCPM_DISPATCH_CASE
    EXPECT "dispatch needs a T that is callable with these arguments. use visit for other values"
    "dispatch on a space fails to compile with a value not callable with the arguments given"
    "dispatch on a const space fails to compile with a value not callable with the arguments given"
)

oscpm_add_compile_failures(oscpm_value_type value_type.cpp OSCPM_VALUE_TYPE_CASE
    EXPECT "AddressSpace needs a T that is an object type, move-constructible and move-assignable"
    "an address space of a value type with a const member fails to compile"
    "an address space of a lambda's closure type fails to compile"
    "an address space of a type that cannot be moved fails to compile"
    "an address space of a reference type fails to compile"
    EXPECT "AddressSpace needs a T whose move construction and move assignment cannot throw"
    "an address space of a type whose move construction may throw fails to compile"
    "an address space of a type whose move assignment may throw fails to compile"
)

oscpm_add_compile_failure(oscpm_cache_bits cache_bits.cpp ""
    "an address space with as many cache bits as std::size_t has bits fails to compile"
    "AddressSpace needs a CacheBits smaller than the number of bits in std::size_t")

oscpm_add_compile_failures(oscpm_expected_type expected_type.cpp OSCPM_EXPECTED_TYPE_CASE
    EXPECT "Expected needs an E that is a scoped enumeration"
    "an expected with an error type that is not an enumeration fails to compile"
    "an expected with an error type that is an unscoped enumeration fails to compile"
    EXPECT "Expected needs a T and an E that do not convert to each other"
    "an expected with a value type that converts from the error type fails to compile"
)

add_test(NAME "an address whose bytes changed after parsing trips an assertion in a build without NDEBUG" COMMAND oscpm_rules change-parsed-address)
set_tests_properties("an address whose bytes changed after parsing trips an assertion in a build without NDEBUG" PROPERTIES
    PASS_REGULAR_EXPRESSION "must stay unchanged after it is parsed")

add_test(NAME "reading the value of an expected holding an error aborts" COMMAND oscpm_rules read-value-of-error)
add_test(NAME "reading the error of an expected holding a value aborts" COMMAND oscpm_rules read-error-of-value)
add_test(NAME "constructing an address from malformed text aborts" COMMAND oscpm_rules construct-address /synth/1/)
add_test(NAME "constructing a pattern from malformed text aborts" COMMAND oscpm_rules construct-pattern "/synth/[1-3")

oscpm_add_compile_failures(oscpm_constructor_constant constructor_constant.cpp OSCPM_CONSTRUCTOR_CASE
    EXPECT "constant expression|evaluate to a constant"
    "an address constructed from malformed text in a constant expression fails to compile"
    "a pattern constructed from malformed text in a constant expression fails to compile"
)

# Clang before 20 accepts the attribute and reports nothing for a view of a
# temporary held through it, so the compiler is asked.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    try_compile(OSCPM_DANGLING_VIEW_PROBE_COMPILES
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/dangling_view_probe.cpp
        COMPILE_DEFINITIONS -Werror=dangling-gsl
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        NO_CACHE
        OUTPUT_VARIABLE OSCPM_DANGLING_VIEW_PROBE_OUTPUT)
    if(NOT OSCPM_DANGLING_VIEW_PROBE_COMPILES AND OSCPM_DANGLING_VIEW_PROBE_OUTPUT MATCHES "Wdangling-gsl\\]")
        oscpm_add_compile_failures(oscpm_dangling_view dangling_view.cpp OSCPM_DANGLING_VIEW_CASE
            COMPILE_OPTION -Werror=dangling-gsl
            EXPECT "object backing the pointer will be destroyed at the end of the full-expression"
            "an address constructed from a temporary string fails to compile"
            "a pattern constructed from a temporary string fails to compile"
            "an address parsed from a temporary string and kept after it fails to compile"
            "a pattern parsed from a temporary string and kept after it fails to compile"
        )
    endif()
endif()

if(OSCPM_SANITIZE)
    foreach(target oscpm_tests oscpm_rules)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()
