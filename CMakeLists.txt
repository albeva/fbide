cmake_minimum_required(VERSION 3.16)
project(fbide)
set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP" )
endif()

#-----------------------------------------------------------------------------------------------------------------------
# wxWidgets
#-----------------------------------------------------------------------------------------------------------------------
SET(WX_ROOT_DIR ${WXWIN})
if(CMAKE_CL_64)
    set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc_x64_dll)
    message(STATUS "Choose 64bit wxWidgets")
else()
    set(wxWidgets_LIB_DIR  ${WX_ROOT_DIR}/lib/vc_dll)
    message(STATUS "Choose x86 wxWidgets")
endif()
set(wxWidgets_ROOT_DIR ${WX_ROOT_DIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(wxWidgets_CONFIGURATION mswud)
else()
    set(wxWidgets_CONFIGURATION mswu)
endif()
find_package(wxWidgets 3.1 COMPONENTS core base aui stc REQUIRED)

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
