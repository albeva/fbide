cmake_minimum_required(VERSION 3.16)
project(fbide)
set(CMAKE_CXX_STANDARD 17)

# wxWidgets
SET(WX_ROOT_DIR "D:/Developer/libs/wxWidgets/wxWidgets-3.1.4")

if(TARGET_64)
    set(wxWidgets_LIB_DIR "D:/Developer/libs/wxWidgets/dist/lib/vc_x64_dll")
    message(STATUS "Choose 64bit wxWidgets")
else()
    set(wxWidgets_LIB_DIR "D:/Developer/libs/wxWidgets/dist/lib/vc_dll")
    message(STATUS "Choose x86 wxWidgets")
endif()
set(wxWidgets_ROOT_DIR ${WX_ROOT_DIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(wxWidgets_CONFIGURATION mswud)
else()
    set(wxWidgets_CONFIGURATION mswu)
endif()
find_package(wxWidgets 3.1 COMPONENTS core base stc REQUIRED)

include(${wxWidgets_USE_FILE})
add_executable(fbide WIN32
    src/inc/pch.h
    src/rc/FBIde0.4_private.rc
    src/inc/globals.h
    src/inc/main.h
    src/main.cpp
    src/inc/AboutDialog.h
    src/AboutDialog.cpp
    src/inc/browser.h
    src/browser.cpp
    src/inc/buffer.h
    src/buffer.cpp
    src/inc/bufferlist.h
    src/bufferlist.cpp
    src/config.cpp
    src/inc/configdiag.h
    src/configdiag.cpp
    src/edit.cpp
    src/fbedit.cpp
    src/file.cpp
    src/inc/FormatDialog.h
    src/FormatDialog.cpp
    src/help.cpp
    src/lang.cpp
    src/loadui.cpp
    src/run.cpp
    src/search.cpp
    src/view.cpp
    src/inc/wxmynotebook.h
    src/wxmynotebook.cpp
    src/inc/InstanceHandler.h src/InstanceHandler.cpp)

# PCH header
target_precompile_headers(fbide INTERFACE src/inc/pch.h)

set_target_properties(fbide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

target_link_libraries(fbide ${wxWidgets_LIBRARIES})