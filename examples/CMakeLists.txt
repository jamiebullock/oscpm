# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

include(FetchContent)

# SYSTEM makes oscpp's headers system includes for the example, so its
# warnings do not fail the build.
FetchContent_Declare(
    oscpp
    GIT_REPOSITORY https://github.com/kaoskorobase/oscpp.git
    GIT_TAG a62fe7690ce3563c997d7c9915d8cb54ff8f79b0 # 1.0.0
    SYSTEM
)
FetchContent_MakeAvailable(oscpp)

add_executable(oscpm_dispatch dispatch.cpp)

target_link_libraries(oscpm_dispatch PRIVATE
    oscpm::oscpm
    oscpp::oscpp
)

target_compile_options(oscpm_dispatch PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

add_test(NAME oscpm_dispatch_example COMMAND oscpm_dispatch)
