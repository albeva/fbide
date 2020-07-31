cmake_minimum_required(VERSION 3.17)

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

#-----------------------------------------------------------------------------------------------------------------------
# wxWidgets
#-----------------------------------------------------------------------------------------------------------------------
if (MSVC)
    SET(WX_ROOT_DIR ${WXWIN})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_CONFIGURATION mswud)
        set(LIB_SUFFIX dll)
        set(WXLIBS core base aui stc)
    else()
        set(wxWidgets_CONFIGURATION mswu)
        set(LIB_SUFFIX lib)
        set(WXLIBS core base aui stc scintilla)
    endif()

    if(CMAKE_CL_64)
        set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_x64_${LIB_SUFFIX})
    else()
        set(wxWidgets_LIB_DIR  ${WX_ROOT_DIR}/lib/vc_${LIB_SUFFIX})
    endif()
    set(wxWidgets_ROOT_DIR ${WX_ROOT_DIR})
else()
    set(WXLIBS core base aui stc)
endif()
find_package(wxWidgets 3.1 COMPONENTS ${WXLIBS} REQUIRED)

#-----------------------------------------------------------------------------------------------------------------------
# YAML
#-----------------------------------------------------------------------------------------------------------------------
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "disable yaml tests" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "disable yaml tools" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "disable yaml contrib" FORCE)
if (MSVC)
    if(LIB_SUFFIX STREQUAL "dll")
        set(YAML_MSVC_SHARED_RT ON CACHE BOOL "Enable shared rt lib" FORCE)
    else()
        set(YAML_MSVC_SHARED_RT OFF CACHE BOOL "Disable shared rt lib" FORCE)
    endif()
endif()
add_subdirectory(third-party/yaml-cpp)

#-----------------------------------------------------------------------------------------------------------------------
# fbide
#-----------------------------------------------------------------------------------------------------------------------

add_subdirectory(fblexer)
add_subdirectory(src)

#-----------------------------------------------------------------------------------------------------------------------
# MSVC
#-----------------------------------------------------------------------------------------------------------------------
if (MSVC)
    if(LIB_SUFFIX STREQUAL "dll")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(RUNTIME_LIB MultiThreadedDebugDLL)
        else()
            set(RUNTIME_LIB MultiThreadedDLL)
        endif()
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(RUNTIME_LIB MultiThreadedDebug)
        else()
            set(RUNTIME_LIB MultiThreaded)
        endif()
    endif()
    set_property(
        TARGET fblexer fbide
        PROPERTY MSVC_RUNTIME_LIBRARY ${RUNTIME_LIB})
endif()
