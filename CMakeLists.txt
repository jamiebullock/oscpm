# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectVersion)
project(oscpm VERSION ${OSCPM_VERSION} LANGUAGES CXX)

add_library(oscpm INTERFACE)
add_library(oscpm::oscpm ALIAS oscpm)

target_include_directories(oscpm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(oscpm INTERFACE cxx_std_17)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS oscpm EXPORT oscpmTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT oscpmTargets
    FILE oscpmConfig.cmake
    NAMESPACE oscpm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm
)
# Semantic versioning: a minor release may break the API while the major is 0.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(OSCPM_VERSION_COMPATIBILITY SameMinorVersion)
else()
    set(OSCPM_VERSION_COMPATIBILITY SameMajorVersion)
endif()
write_basic_package_version_file(
    oscpmConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${OSCPM_VERSION_COMPATIBILITY}
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm
)

option(OSCPM_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_CHECK_FORMAT "Fail the build on clang-format violations" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_BUILD_FUZZERS "Build the libFuzzer target under sanitizers; needs a compiler that ships libFuzzer" OFF)
option(OSCPM_SANITIZE "Build the tests under AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(OSCPM_CHECK_FORMAT)
    include(CheckFormat)
    file(GLOB_RECURSE OSCPM_FORMATTED_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp
    )
    add_clang_format_check(SOURCES ${OSCPM_FORMATTED_SOURCES})
endif()

if(OSCPM_BUILD_TESTS OR OSCPM_BUILD_FUZZERS)
    include(CTest)
endif()

if(OSCPM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(OSCPM_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
