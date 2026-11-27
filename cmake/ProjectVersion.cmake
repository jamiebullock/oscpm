# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# The project version from the most recent v* tag of the repository whose root
# is oscpm's source directory, or OSCPM_VERSION when the caller sets it for a
# build from an archive. The variable is OSCPM_VERSION because a parent
# project() call has already set PROJECT_VERSION when oscpm is a subproject.

if(NOT DEFINED OSCPM_VERSION)
    find_package(Git QUIET)

    if(Git_FOUND)
        # GIT_DIR and GIT_WORK_TREE select a repository whatever the working
        # directory, so git runs without them.
        set(OSCPM_GIT "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE "${GIT_EXECUTABLE}")

        # The path from the repository's root to oscpm's source directory. It
        # is empty in a clone and is not when an archive of oscpm has been
        # unpacked inside another repository, whose tags are not oscpm's.
        execute_process(
            COMMAND ${OSCPM_GIT} rev-parse --show-prefix
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
            OUTPUT_VARIABLE OSCPM_GIT_PREFIX
            RESULT_VARIABLE OSCPM_GIT_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(OSCPM_GIT_RESULT EQUAL 0 AND OSCPM_GIT_PREFIX STREQUAL "")
            execute_process(
                COMMAND ${OSCPM_GIT} describe --tags --abbrev=0 --match "v[0-9]*"
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
                OUTPUT_VARIABLE OSCPM_GIT_TAG
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
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
