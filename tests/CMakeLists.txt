include(FetchContent)

# Catch2 defaults to C++14, which leaves out its std::string_view support.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(oscpm_tests
    address_space_test.cpp
    allocation_counter.cpp
    corpus.cpp
    corpus_test.cpp
    pattern_test.cpp
    wildcard_lookup_test.cpp)

target_link_libraries(oscpm_tests PRIVATE oscpm::oscpm Catch2::Catch2WithMain)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt")

oscpm_strict_warnings(oscpm_tests)
catch_discover_tests(oscpm_tests)

# The adapter's smoke test is a separate binary so that the core suite never
# includes oscpp; it is skipped when the adapter target is absent.
if(TARGET oscpm::oscpp)
    add_executable(oscpm_oscpp_tests
        allocation_counter.cpp
        oscpp_adapter_test.cpp)

    target_link_libraries(oscpm_oscpp_tests PRIVATE oscpm::oscpp Catch2::Catch2WithMain)

    oscpm_strict_warnings(oscpm_oscpp_tests)
    catch_discover_tests(oscpm_oscpp_tests)
endif()
