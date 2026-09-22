# Catch2 must be built as C++17 so its std::string_view support is compiled in.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

set(oscpm_warnings -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(oscpm_test_sources test_pattern.cpp test_address_space.cpp)
if(TARGET oscpp::oscpp)
    list(APPEND oscpm_test_sources test_oscpp.cpp)
endif()

add_executable(oscpm_tests ${oscpm_test_sources})
target_link_libraries(oscpm_tests PRIVATE oscpm::oscpm Catch2::Catch2WithMain)
if(TARGET oscpp::oscpp)
    target_link_libraries(oscpm_tests PRIVATE oscpp::oscpp)
endif()
target_compile_options(oscpm_tests PRIVATE
    "$<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:${oscpm_warnings}>")

include(Catch)
catch_discover_tests(oscpm_tests)
