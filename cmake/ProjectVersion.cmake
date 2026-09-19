# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# The project version, taken from the most recent git tag. Tags are the only
# place a released version is written down, so nothing in the tree can
# disagree with them. A build between releases reports the release it
# followed, which is what `git describe --abbrev=0` gives.
#
# OSCPM_VERSION may be set by the caller, for a build from an archive with no
# repository to ask. It has a name of its own because a parent project's
# project() call has already set PROJECT_VERSION when oscpm is added to
# another build.

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
        # A shallow clone, a checkout with no tags fetched, or an archive. The
        # build works; it just cannot say which release it is.
        set(OSCPM_VERSION "0.0.0")
        message(WARNING "No version tag found: building as ${OSCPM_VERSION}. "
                        "A clone needs its tags for the version to be real.")
    endif()

    message(STATUS "Project version set to ${OSCPM_VERSION}")
endif()
