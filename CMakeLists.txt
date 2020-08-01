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
    option(FBIDE_USE_SHARED_LIBS "Use shared runtime library" OFF)
    if(FBIDE_USE_SHARED_LIBS)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL)
        else()
            set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
        else()
            set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
        endif()
    endif()
endif()

#-----------------------------------------------------------------------------------------------------------------------
# wxWidgets
#-----------------------------------------------------------------------------------------------------------------------
if (MSVC)
    SET(WX_ROOT_DIR ${WXWIN})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_CONFIGURATION mswud)
        set(WXLIBS core base aui stc)
    else()
        set(wxWidgets_CONFIGURATION mswu)
        set(WXLIBS core base aui stc scintilla)
    endif()

    if(FBIDE_USE_SHARED_LIBS)
        set(WX_LIB_SUFFIX dll)
    else()
        set(WX_LIB_SUFFIX lib)
    endif()

    if(CMAKE_CL_64)
        set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_x64_${WX_LIB_SUFFIX})
    else()
        set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_${WX_LIB_SUFFIX})
    endif()
    set(wxWidgets_ROOT_DIR ${WX_ROOT_DIR})
else()
    set(WXLIBS core base aui stc)
endif()
find_package(wxWidgets 3.1 COMPONENTS ${WXLIBS} REQUIRED)

#-----------------------------------------------------------------------------------------------------------------------
# YAML
#-----------------------------------------------------------------------------------------------------------------------
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_BUILD_CONTRIB OFF)
if(FBIDE_USE_SHARED_LIBS)
    set(YAML_MSVC_SHARED_RT ON)
else()
    set(YAML_MSVC_SHARED_RT OFF)
endif()
add_subdirectory(third-party/yaml-cpp)

#-----------------------------------------------------------------------------------------------------------------------
# fbide
#-----------------------------------------------------------------------------------------------------------------------

add_subdirectory(fblexer)
add_subdirectory(src)
