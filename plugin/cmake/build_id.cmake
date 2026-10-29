# [Claude 2026-10-29] Writes build_id.h before every build: the commit, whether the build had uncommitted changes, and
# when it was built. The About window shows it on Ctrl+Shift+click of the version, so Joe can confirm he is testing
# the exact build just made (Joe: "It would be nice to have some way of seeing the exact build number").
# Run as: cmake -DOUT=<file> -DSRC=<repo dir> -P build_id.cmake
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain --untracked-files=no WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if (NOT COMMIT)
    set(COMMIT "unknown")
endif()
if (DIRTY)
    set(COMMIT "${COMMIT}+")
endif()
string(TIMESTAMP WHEN "%Y-%m-%d %H:%M")
set(TEXT "#pragma once\n#define WLA_BUILD_ID \"${COMMIT} · ${WHEN}\"\n")
if (EXISTS "${OUT}")
    file(READ "${OUT}" OLD)
endif()
if (NOT "${OLD}" STREQUAL "${TEXT}")
    file(WRITE "${OUT}" "${TEXT}")
endif()
