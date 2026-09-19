cmake_minimum_required(VERSION 3.14)

project(oscpm
    VERSION 0.1.0
    DESCRIPTION "Header-only C++17 OSC address pattern matching"
    LANGUAGES CXX)

add_library(oscpm INTERFACE)
add_library(oscpm::oscpm ALIAS oscpm)

target_include_directories(oscpm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(oscpm INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OSCPM_IS_TOP_LEVEL ON)
else()
    set(OSCPM_IS_TOP_LEVEL OFF)
endif()

option(OSCPM_BUILD_TESTS "Build the oscpm test suite" ${OSCPM_IS_TOP_LEVEL})
option(OSCPM_BUILD_EXAMPLES "Build the oscpm examples" ${OSCPM_IS_TOP_LEVEL})
option(OSCPM_FETCH_OSCPP
    "Pull oscpp with FetchContent when no oscpp is found, so that the adapter, its test and the example build"
    ${OSCPM_IS_TOP_LEVEL})

# The oscpp adapter target, oscpm::oscpp, exists only when oscpp is located:
# as a target the enclosing project already created, as an installed
# package, or, with OSCPM_FETCH_OSCPP, by FetchContent. It is absent
# otherwise, and the core target above never depends on it.
if(NOT TARGET oscpp::oscpp)
    find_package(oscpp QUIET CONFIG)
endif()

if(NOT TARGET oscpp::oscpp AND OSCPM_FETCH_OSCPP)
    include(FetchContent)
    FetchContent_Declare(
        oscpp
        GIT_REPOSITORY https://github.com/kaoskorobase/oscpp.git
        GIT_TAG        1.0.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(oscpp)
endif()

if(TARGET oscpp::oscpp)
    add_library(oscpm_oscpp INTERFACE)
    add_library(oscpm::oscpp ALIAS oscpm_oscpp)
    set_target_properties(oscpm_oscpp PROPERTIES EXPORT_NAME oscpp)
    target_link_libraries(oscpm_oscpp INTERFACE oscpm::oscpm oscpp::oscpp)
endif()

# The warning level every test and example is built at (story 47): the
# headers must inherit no warnings into a consumer's project.
function(oscpm_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(OSCPM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OSCPM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
