cmake_minimum_required(VERSION 3.16)

# Set the project name to your project name
project(fbide CXX)
include(cmake/StandardProjectSettings.cmake)
include(cmake/PreventInSourceBuilds.cmake)

# Link this 'library' to set the c++ standard / compileSource-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# allow for static analysis options
include(cmake/StaticAnalyzers.cmake)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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
endif()
set(WXLIBS core base aui stc scintilla)
find_package(wxWidgets 3.1 COMPONENTS ${WXLIBS} REQUIRED)

#-----------------------------------------------------------------------------------------------------------------------
# fbide
#-----------------------------------------------------------------------------------------------------------------------
add_subdirectory(src)
