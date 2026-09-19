# A consumer of oscpm, built as a separate project so that the tests prove
# what a user sees rather than what the oscpm tree sees. It links both the
# core and the adapter target by one of the two documented routes:
#
#   OSCPM_CONSUMER_ROUTE=install  find_package(oscpm) against an install prefix
#   OSCPM_CONSUMER_ROUTE=fetch    FetchContent_MakeAvailable(oscpm)
#
# tests/consumer/run.cmake drives it from ctest; see there for the arguments.
cmake_minimum_required(VERSION 3.24)

project(oscpm_consumer LANGUAGES CXX)

set(OSCPM_CONSUMER_ROUTE "install" CACHE STRING "install or fetch")
set(OSCPM_CONSUMER_OSCPM_SOURCE "" CACHE PATH
    "With the fetch route, an oscpm checkout to use instead of cloning")

if(OSCPM_CONSUMER_ROUTE STREQUAL "install")
    find_package(oscpm 0.1 CONFIG REQUIRED COMPONENTS oscpp)
elseif(OSCPM_CONSUMER_ROUTE STREQUAL "fetch")
    include(FetchContent)
    # A consumer asks for the adapter by turning OSCPM_FETCH_OSCPP on, which
    # is off by default when oscpm is not the top-level project.
    set(OSCPM_FETCH_OSCPP ON)
    if(OSCPM_CONSUMER_OSCPM_SOURCE)
        set(FETCHCONTENT_SOURCE_DIR_OSCPM ${OSCPM_CONSUMER_OSCPM_SOURCE})
    endif()
    FetchContent_Declare(oscpm
        GIT_REPOSITORY https://github.com/jamiebullock/oscpm.git
        GIT_TAG        0.1.0)
    FetchContent_MakeAvailable(oscpm)
else()
    message(FATAL_ERROR "OSCPM_CONSUMER_ROUTE must be install or fetch, not ${OSCPM_CONSUMER_ROUTE}")
endif()

foreach(target oscpm::oscpm oscpm::oscpp)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "${target} is missing after the ${OSCPM_CONSUMER_ROUTE} route")
    endif()
endforeach()

add_executable(consumer_core core.cpp)
target_link_libraries(consumer_core PRIVATE oscpm::oscpm)

add_executable(consumer_adapter adapter.cpp)
target_link_libraries(consumer_adapter PRIVATE oscpm::oscpp)

# The consumer is built at the same warning level as oscpm's own tests, so
# that the installed headers are shown to be warning-free too.
foreach(target consumer_core consumer_adapter)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

# Both programs exit 0 on success, so ctest runs them as tests.
enable_testing()
add_test(NAME consumer_core COMMAND consumer_core)
add_test(NAME consumer_adapter COMMAND consumer_adapter)
