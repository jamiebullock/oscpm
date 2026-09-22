cmake_minimum_required(VERSION 3.24)
project(oscpm VERSION 0.1.0 LANGUAGES CXX)

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
write_basic_package_version_file(
    oscpmConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/oscpmConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpm
)

option(OSCPM_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(OSCPM_WITH_OSCPP "Fetch oscpp and build the oscpp integration tests and examples" ON)

if(OSCPM_WITH_OSCPP AND (OSCPM_BUILD_TESTS OR OSCPM_BUILD_EXAMPLES))
    include(FetchContent)
    FetchContent_Declare(oscpp
        GIT_REPOSITORY https://github.com/kaoskorobase/oscpp.git
        GIT_TAG        a62fe7690ce3563c997d7c9915d8cb54ff8f79b0
        GIT_SHALLOW    TRUE
    )
    set(OSCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(oscpp)
    # Treat oscpp's headers as system headers so our warning flags apply only to oscpm code.
    get_target_property(oscpp_include_dirs oscpp INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(oscpp PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${oscpp_include_dirs}")
endif()

if(OSCPM_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

if(OSCPM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
