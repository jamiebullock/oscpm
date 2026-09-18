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

add_executable(oscpm_tests
    allocation_counter.cpp
    corpus_test.cpp
    pattern_test.cpp)

target_link_libraries(oscpm_tests PRIVATE oscpm::oscpm Catch2::Catch2WithMain)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt")

if(MSVC)
    target_compile_options(oscpm_tests PRIVATE /W4)
else()
    target_compile_options(oscpm_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(oscpm_tests)
