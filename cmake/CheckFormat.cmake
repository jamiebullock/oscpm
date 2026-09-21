# Part of oscpm-regex
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# add_clang_format_check(SOURCES <files>...)
#
# Adds `check-format`, part of the default build, which fails when any of the
# files differs from what clang-format would produce, and `format`, which
# rewrites them in place. Requires clang-format on the PATH.

function(add_clang_format_check)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "add_clang_format_check(): SOURCES is required")
    endif()

    find_program(CLANG_FORMAT_EXE NAMES clang-format REQUIRED)

    add_custom_target(check-format ALL
        COMMAND ${CLANG_FORMAT_EXE} --style=file --dry-run --Werror ${ARG_SOURCES}
        COMMENT "Checking formatting with clang-format"
        VERBATIM
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} --style=file -i ${ARG_SOURCES}
        COMMENT "Applying clang-format"
        VERBATIM
    )
endfunction()
