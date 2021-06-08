# fbide binary
add_executable(fbide WIN32
    inc/pch.h
    rc/FBIde0.4_private.rc
    inc/globals.h
    inc/FBIdeMainFrame.h
    FBIdeMainFrame.cpp
    inc/AboutDialog.h
    AboutDialog.cpp
    inc/browser.h
    browser.cpp
    inc/buffer.h
    buffer.cpp
    inc/bufferlist.h
    bufferlist.cpp
    config.cpp
    inc/configdiag.h
    configdiag.cpp
    edit.cpp
    fbedit.cpp
    file.cpp
    inc/FormatDialog.h
    FormatDialog.cpp
    help.cpp
    lang.cpp
    loadui.cpp
    run.cpp
    search.cpp
    view.cpp
    inc/wxmynotebook.h
    wxmynotebook.cpp
    inc/InstanceHandler.h
    InstanceHandler.cpp
    inc/FBIdeApp.h
    FBIdeApp.cpp)

# PCH file
target_precompile_headers(fbide PRIVATE inc/pch.h)

# emit exe to bin/
set_target_properties(fbide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# set src root for includes
target_include_directories(fbide PRIVATE
    ${PROJECT_SOURCE_DIR}/wxSTC/include
    ${fbide_SOURCE_DIR})

# wxWidgets
include(${wxWidgets_USE_FILE})

# link dependencies
target_link_libraries(fbide
    PRIVATE
    ${wxWidgets_LIBRARIES}
    project_options project_warnings)
