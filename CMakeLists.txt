cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Single source of truth for the FBIde version. Bump this per release.
#   fbide_version(0.5.0)            -> final
#   fbide_version(0.5.0 RC 1)       -> 0.5.0.rc-1
#   fbide_version(0.5.0 Beta 2)     -> 0.5.0.beta-2
include(cmake/version.cmake)
fbide_version(0.5.0 Beta 2)

# Version probe: CI runs `cmake -B <dir> -DFBIDE_VERSION_ONLY=ON .` to
# resolve the configured version without the full configure (no compiler
# detection, no wxWidgets, no subdirectories). The version stamp is
# written on this fast path; the matrix build also writes it later from
# configured_files/CMakeLists.txt for the regular configure path.
if (FBIDE_VERSION_ONLY)
    project(fbide LANGUAGES NONE)
    file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${FBIDE_FULL_VERSION}")
    return()
endif()

# The Project
project(
    fbide
    VERSION ${FBIDE_VERSION_NUMERIC}
    DESCRIPTION "An open-source IDE for the FreeBASIC compiler"
    HOMEPAGE_URL "https://github.com/albeva/fbide"
    LANGUAGES CXX
)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Enable CTest at the top level so gtest_discover_tests() registrations in
# tests/CMakeLists.txt populate CTestTestfile.cmake — without this, ctest
# reports "No tests were found" even though the tests binary builds and runs.
enable_testing()

# CMake modules
include(FetchContent)
include(cmake/dependencies.cmake)
include(cmake/warnings.cmake)
include(cmake/options.cmake)
include(cmake/wxwidgets.cmake)
include(cmake/lexilla.cmake)
include(cmake/doxygen.cmake)

# Sources
add_subdirectory(configured_files)
add_subdirectory(src)
add_subdirectory(tests)

# ----------------------------------------------------------------------------
# Install layout (consumed by `cmake --install <build> --prefix <staging>`)
#
#   <prefix>/
#     fbide.exe
#     ide/
#       config_*.ini, themes/, locales/, ...   (verbatim from resources/pristine)
#
# The release workflow zips the staging directory directly; no sanitiser
# script. Run locally to reproduce the installer payload.
# ----------------------------------------------------------------------------
install(TARGETS fbide RUNTIME DESTINATION .)
install(DIRECTORY resources/pristine/ DESTINATION ide)
