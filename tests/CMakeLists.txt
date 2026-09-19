find_package(Catch2 3 CONFIG QUIET)
if(NOT Catch2_FOUND)
    # Build the fetched Catch2 as C++17 so its std::string_view support matches ours.
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

add_executable(oscpm_tests test_match.cpp)
target_link_libraries(oscpm_tests PRIVATE oscpm::oscpm Catch2::Catch2WithMain)
target_compile_options(oscpm_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

include(Catch)
catch_discover_tests(oscpm_tests)
