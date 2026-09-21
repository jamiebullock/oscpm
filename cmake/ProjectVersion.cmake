# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# The project version from the most recent v* tag, or OSCPM_VERSION when the
# caller sets it for a build from an archive. Its own name, because a parent
# project() call has already set PROJECT_VERSION when oscpm is a subproject.

if(NOT DEFINED OSCPM_VERSION)
    find_package(Git QUIET)

    if(Git_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            OUTPUT_VARIABLE OSCPM_GIT_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(OSCPM_GIT_TAG)
            string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" OSCPM_VERSION "${OSCPM_GIT_TAG}")
        endif()
    endif()

    if(NOT OSCPM_VERSION OR OSCPM_VERSION STREQUAL "")
        set(OSCPM_VERSION "0.0.0")
        message(WARNING "No version tag found: building as ${OSCPM_VERSION}. "
                        "A clone needs its tags for the version to be real.")
    endif()

    message(STATUS "Project version set to ${OSCPM_VERSION}")
endif()
