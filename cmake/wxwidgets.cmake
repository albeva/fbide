# wxWidgets configuration
set(FBIDE_WX_COMPONENTS core base aui stc net)
find_package(wxWidgets 3.3 COMPONENTS ${FBIDE_WX_COMPONENTS} REQUIRED)
include(${wxWidgets_USE_FILE})
