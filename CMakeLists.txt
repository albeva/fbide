cmake_minimum_required(VERSION 4.2.2)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# The Project
project(
    fbide
    VERSION 0.5.0
    DESCRIPTION "An open-source IDE for the FreeBASIC compiler"
    HOMEPAGE_URL "https://github.com/albeva/fbide"
    LANGUAGES CXX
)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# CMake modules
include(FetchContent)
include(cmake/warnings.cmake)
include(cmake/options.cmake)
include(cmake/wxwidgets.cmake)

# Sources
add_subdirectory(src)
add_subdirectory(tests)
