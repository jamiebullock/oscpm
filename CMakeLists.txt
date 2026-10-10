# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

cmake_minimum_required(VERSION 3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake")
project(oscpm VERSION ${OSCPM_VERSION} LANGUAGES CXX)

add_library(oscpm INTERFACE)
add_library(oscpm::oscpm ALIAS oscpm)

target_include_directories(oscpm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(oscpm INTERFACE cxx_std_17)

if(PROJECT_IS_TOP_LEVEL)
    file(GLOB_RECURSE OSCPM_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
    target_sources(oscpm PRIVATE ${OSCPM_HEADERS} README.md)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include FILES ${OSCPM_HEADERS})
    source_group("" FILES README.md)
endif()

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

if(PROJECT_IS_TOP_LEVEL)
    option(BUILD_TESTING "Build and register the tests" ON)
endif()
option(OSCPM_BUILD_EXAMPLES "Build the example, which fetches oscpp" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_CHECK_FORMAT "Fail the build on clang-format violations" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_BUILD_BENCHMARKS "Build the benchmarks, which fetch Google Benchmark" OFF)
option(OSCPM_BUILD_FUZZERS "Build the libFuzzer target under sanitizers; needs a compiler that ships libFuzzer" OFF)
option(OSCPM_SANITIZE "Build the tests under AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(OSCPM_MULL_PLUGIN "" CACHE FILEPATH "Mull IR plugin to compile the tests with; adds the mutation target")

if(OSCPM_CHECK_FORMAT)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckFormat.cmake")
    file(GLOB_RECURSE OSCPM_FORMATTED_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.ino
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp
    )
    add_clang_format_check(SOURCES ${OSCPM_FORMATTED_SOURCES})
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OSCPM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(OSCPM_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

if(OSCPM_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
