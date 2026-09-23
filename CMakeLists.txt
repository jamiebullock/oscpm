# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectVersion)
project(oscpm_flat VERSION ${OSCPM_VERSION} LANGUAGES CXX)

add_library(oscpm_flat INTERFACE)
add_library(oscpm_flat::oscpm_flat ALIAS oscpm_flat)

target_include_directories(oscpm_flat INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(oscpm_flat INTERFACE cxx_std_17)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS oscpm_flat EXPORT oscpm_flatTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT oscpm_flatTargets
    FILE oscpm_flatConfig.cmake
    NAMESPACE oscpm_flat::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm_flat
)
write_basic_package_version_file(
    oscpm_flatConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/oscpm_flatConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm_flat
)

option(OSCPM_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_CHECK_FORMAT "Fail the build on clang-format violations" ${PROJECT_IS_TOP_LEVEL})

if(OSCPM_CHECK_FORMAT)
    include(CheckFormat)
    file(GLOB_RECURSE OSCPM_FORMATTED_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    add_clang_format_check(SOURCES ${OSCPM_FORMATTED_SOURCES})
endif()

if(OSCPM_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
