cmake_minimum_required(VERSION 3.14)

project(oscpm
    VERSION 0.1.0
    DESCRIPTION "OSC address pattern matching for C++17"
    LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(oscpm INTERFACE)
add_library(oscpm::oscpm ALIAS oscpm)

target_include_directories(oscpm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(oscpm INTERFACE cxx_std_17)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(OSCPM_IS_TOP_LEVEL ON)
else()
    set(OSCPM_IS_TOP_LEVEL OFF)
endif()

option(OSCPM_BUILD_TESTS "Build the oscpm test suite" ${OSCPM_IS_TOP_LEVEL})
option(OSCPM_INSTALL "Generate install rules" ${OSCPM_IS_TOP_LEVEL})

if(OSCPM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OSCPM_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS oscpm EXPORT oscpmTargets)
    install(EXPORT oscpmTargets
        NAMESPACE oscpm::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfig.cmake
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/oscpmTargets.cmake\")\n")
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm)
endif()
