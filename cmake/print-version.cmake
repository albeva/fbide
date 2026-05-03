# Resolve FBIde's full version from fbide_version() in CMakeLists.txt
# without running a project configure. Used by CI to compute the release
# version once per workflow run, ahead of the matrix builds.
#
# Invocation:
#     cmake -DVERSION_OUT=<path> -P cmake/print-version.cmake
#
# `VERSION_OUT` defaults to ./version.txt relative to the working
# directory. The resolved version is also echoed via STATUS for log
# visibility.

cmake_minimum_required(VERSION 3.20)

get_filename_component(_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")

# Pull the first non-comment fbide_version(...) call. The example lines
# in the comment block above the call also contain `fbide_version(...)`
# tokens, so anchor to start-of-line to skip anything prefixed with `#`.
file(STRINGS "${_repo}/CMakeLists.txt" _lines
     REGEX "^[ \t]*fbide_version[ \t]*\\(")
list(LENGTH _lines _n)
if (_n EQUAL 0)
    message(FATAL_ERROR "fbide_version(...) call not found in CMakeLists.txt")
endif()
list(GET _lines 0 _line)
if (NOT _line MATCHES "fbide_version[ \t]*\\(([^)]*)\\)")
    message(FATAL_ERROR "Could not parse fbide_version line: ${_line}")
endif()
separate_arguments(_args NATIVE_COMMAND "${CMAKE_MATCH_1}")
fbide_version(${_args})

if (NOT DEFINED VERSION_OUT)
    set(VERSION_OUT "version.txt")
endif()
file(WRITE "${VERSION_OUT}" "${FBIDE_FULL_VERSION}")
message(STATUS "FBIde version: ${FBIDE_FULL_VERSION}")
