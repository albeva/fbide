cmake_minimum_required(VERSION 3.16)

# Set the project name to your project name
project(fbide CXX)
include(cmake/StandardProjectSettings.cmake)
include(cmake/PreventInSourceBuilds.cmake)

# Link this 'library' to set the c++ standard / compileSource-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    option(ENABLE_BUILD_WITH_TIME_TRACE "Enable -ftime-trace to generate time tracing .json files on clang" OFF)
    if(ENABLE_BUILD_WITH_TIME_TRACE)
        add_compile_definitions(project_options INTERFACE -ftime-trace)
    endif()
endif()

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

# sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

# allow for static analysis options
include(cmake/StaticAnalyzers.cmake)

if (MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()
endif()

#-----------------------------------------------------------------------------------------------------------------------
# wxWidgets
#-----------------------------------------------------------------------------------------------------------------------
if (MSVC)
    SET(WX_ROOT_DIR ${WXWIN})
    SET(wxWidgets_ROOT_DIR ${WXWIN})
    if(CMAKE_CL_64)
        set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_x64_lib)
    else()
        set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_lib)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_CONFIGURATION mswud)
    else()
        set(wxWidgets_CONFIGURATION mswu)
    endif()
    set(WXLIBS core base aui scintilla)
else()
    set(WXLIBS core base aui)
endif()
find_package(wxWidgets 3.1 COMPONENTS ${WXLIBS} REQUIRED)

#-----------------------------------------------------------------------------------------------------------------------
# YAML
#-----------------------------------------------------------------------------------------------------------------------
set(YAML_DIST_PATH "" CACHE STRING "cpp-yaml path")
find_package(yaml-cpp CONFIG REQUIRED PATHS ${YAML_DIST_PATH})

#-----------------------------------------------------------------------------------------------------------------------
# fbide
#-----------------------------------------------------------------------------------------------------------------------
add_subdirectory(wxSTC)
add_subdirectory(src)
