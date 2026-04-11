# wxWidgets configuration
if(MSVC)
    set(WX_ROOT_DIR $ENV{WXWIN})
    set(wxWidgets_ROOT_DIR $ENV{WXWIN})
    set(wxWidgets_LIB_DIR ${WX_ROOT_DIR}/lib/vc14x_x64_dll)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_CONFIGURATION mswud)
    else()
        set(wxWidgets_CONFIGURATION mswu)
    endif()
endif()

set(FBIDE_WX_COMPONENTS core base aui stc net)
find_package(wxWidgets 3.3 COMPONENTS ${FBIDE_WX_COMPONENTS} REQUIRED)
include(${wxWidgets_USE_FILE})
