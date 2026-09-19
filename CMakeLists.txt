cmake_minimum_required(VERSION 3.14)

project(oscpm
    VERSION 0.1.0
    DESCRIPTION "Header-only C++17 OSC address pattern matching"
    LANGUAGES CXX)

# The version macros in include/oscpm/version.hpp are the version a consumer
# sees; the project version above is the one CMake installs and tags carry.
# They are kept by hand and must agree, so a mismatch fails at configure.
file(STRINGS include/oscpm/version.hpp OSCPM_VERSION_LINES REGEX "#define OSCPM_VERSION_")
string(REGEX MATCH "OSCPM_VERSION_MAJOR ([0-9]+)" _ "${OSCPM_VERSION_LINES}")
set(OSCPM_HEADER_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "OSCPM_VERSION_MINOR ([0-9]+)" _ "${OSCPM_VERSION_LINES}")
string(APPEND OSCPM_HEADER_VERSION ".${CMAKE_MATCH_1}")
string(REGEX MATCH "OSCPM_VERSION_PATCH ([0-9]+)" _ "${OSCPM_VERSION_LINES}")
string(APPEND OSCPM_HEADER_VERSION ".${CMAKE_MATCH_1}")
if(NOT OSCPM_HEADER_VERSION VERSION_EQUAL PROJECT_VERSION)
    message(FATAL_ERROR
        "include/oscpm/version.hpp says ${OSCPM_HEADER_VERSION} but CMakeLists.txt says "
        "${PROJECT_VERSION}; update one to match the other.")
endif()

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
option(OSCPM_BUILD_FUZZERS
    "Build the libFuzzer target under sanitizers; needs a compiler that ships libFuzzer"
    OFF)
option(OSCPM_WARNINGS_AS_ERRORS
    "Treat warnings as errors in the tests, examples and fuzz target; CI turns this on"
    OFF)
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
        if(OSCPM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(OSCPM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Install and export. The core target is always exported. The adapter target
# goes into a second export set, included by the package config only when
# oscpp can be found at the consumer's configure time, so that
# find_package(oscpm) succeeds without oscpp and a consumer that wants the
# adapter can insist on it with COMPONENTS oscpp.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(OSCPM_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm
    CACHE STRING "Where the oscpm CMake package config is installed, relative to the prefix")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS oscpm EXPORT oscpmTargets)
install(EXPORT oscpmTargets
    NAMESPACE oscpm::
    DESTINATION ${OSCPM_INSTALL_CMAKEDIR})

if(TARGET oscpm::oscpp)
    install(TARGETS oscpm_oscpp EXPORT oscpmOscppTargets)
    install(EXPORT oscpmOscppTargets
        NAMESPACE oscpm::
        DESTINATION ${OSCPM_INSTALL_CMAKEDIR})
endif()

configure_package_config_file(
    cmake/oscpmConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfig.cmake
    INSTALL_DESTINATION ${OSCPM_INSTALL_CMAKEDIR})

# Semantic versioning: while the major version is 0, a minor release may
# break the API, so a consumer's requested version must match on minor too.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(OSCPM_VERSION_COMPATIBILITY SameMinorVersion)
else()
    set(OSCPM_VERSION_COMPATIBILITY SameMajorVersion)
endif()
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${OSCPM_VERSION_COMPATIBILITY}
    ARCH_INDEPENDENT)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
    DESTINATION ${OSCPM_INSTALL_CMAKEDIR})

if(OSCPM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OSCPM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(OSCPM_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
