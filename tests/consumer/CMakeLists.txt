# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectVersion)
project(oscpm_consumer VERSION ${CONSUMER_VERSION} LANGUAGES CXX)

if(DEFINED OSCPM_SOURCE_DIR)
    add_subdirectory(${OSCPM_SOURCE_DIR} oscpm)
else()
    find_package(oscpm REQUIRED)
endif()

add_executable(oscpm_consumer main.cpp)
target_compile_features(oscpm_consumer PRIVATE cxx_std_17)
target_link_libraries(oscpm_consumer PRIVATE oscpm::oscpm)
