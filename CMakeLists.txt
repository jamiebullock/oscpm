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

if(OSCPM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
