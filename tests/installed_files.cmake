# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

# Installs the build tree BUILD_DIR to PREFIX and fails unless the files
# installed are oscpm's headers under INCLUDE_DIR and its package files under
# LIBRARY_DIR. CONFIG names the configuration to install.

set(expected
    ${INCLUDE_DIR}/oscpm/address.h
    ${INCLUDE_DIR}/oscpm/address_space.h
    ${INCLUDE_DIR}/oscpm/detail/address_index.h
    ${INCLUDE_DIR}/oscpm/detail/compiler.h
    ${INCLUDE_DIR}/oscpm/detail/dispatch_memo.h
    ${INCLUDE_DIR}/oscpm/detail/match.h
    ${INCLUDE_DIR}/oscpm/detail/method_table.h
    ${INCLUDE_DIR}/oscpm/detail/prepared.h
    ${INCLUDE_DIR}/oscpm/detail/reach.h
    ${INCLUDE_DIR}/oscpm/detail/string_view.h
    ${INCLUDE_DIR}/oscpm/detail/syntax.h
    ${INCLUDE_DIR}/oscpm/detail/validate.h
    ${INCLUDE_DIR}/oscpm/detail/word.h
    ${INCLUDE_DIR}/oscpm/error.h
    ${INCLUDE_DIR}/oscpm/expected.h
    ${INCLUDE_DIR}/oscpm/oscpm.h
    ${INCLUDE_DIR}/oscpm/pattern.h
    ${LIBRARY_DIR}/cmake/oscpm/oscpmConfig.cmake
    ${LIBRARY_DIR}/cmake/oscpm/oscpmConfigVersion.cmake
)

file(REMOVE_RECURSE "${PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=DESTDIR
        "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}" --config "${CONFIG}"
    OUTPUT_QUIET
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "cmake --install failed: ${result}")
endif()

file(GLOB_RECURSE installed RELATIVE "${PREFIX}" "${PREFIX}/*")
list(SORT installed)
list(SORT expected)

if(NOT installed STREQUAL expected)
    set(unexpected ${installed})
    list(REMOVE_ITEM unexpected ${expected})
    set(missing ${expected})
    list(REMOVE_ITEM missing ${installed})
    list(JOIN unexpected "\n  " unexpected)
    list(JOIN missing "\n  " missing)
    message(FATAL_ERROR "The install differs from oscpm's own files.\nNot expected:\n  ${unexpected}\nMissing:\n  ${missing}")
endif()
