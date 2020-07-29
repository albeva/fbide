cmake_minimum_required(VERSION 3.16)
project(fbide)
set(CMAKE_CXX_STANDARD 17)

#-----------------------------------------------------------------------------------------------------------------------
# wxWidgets
#-----------------------------------------------------------------------------------------------------------------------
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

find_package(wxWidgets 3.1 COMPONENTS ${WXLIBS} REQUIRED)

#-----------------------------------------------------------------------------------------------------------------------
# YAML
#-----------------------------------------------------------------------------------------------------------------------
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "disable yaml tests")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "disable yaml tools")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "disable yaml contrib")
add_subdirectory(third-party/yaml-cpp)

# lex lib
add_subdirectory(fblexer)

# Main App
add_subdirectory(src)

#-----------------------------------------------------------------------------------------------------------------------
# MSVC
#-----------------------------------------------------------------------------------------------------------------------
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP" )
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
        TARGET yaml-cpp fblexer fbide
        PROPERTY MSVC_RUNTIME_LIBRARY ${RUNTIME_LIB})
endif()
