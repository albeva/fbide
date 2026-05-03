cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Single source of truth for the FBIde version. Bump this per release.
#   fbide_version(0.5.0)            -> final
#   fbide_version(0.5.0 RC 1)       -> 0.5.0.rc-1
#   fbide_version(0.5.0 Beta 2)     -> 0.5.0.beta-2
include(cmake/version.cmake)
fbide_version(0.5.0 Beta 1)

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
