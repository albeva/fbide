# fbide binary
add_executable(fbide WIN32
    inc/pch.h
    rc/FBIde0.4_private.rc
    AboutDialog.cpp
    FBIdeApp.cpp
    FBIdeMainFrame.cpp
    FormatDialog.cpp
    InstanceHandler.cpp
    browser.cpp
    buffer.cpp
    bufferlist.cpp
    config.cpp
    configdiag.cpp
    edit.cpp
    fbedit.cpp
    file.cpp
    help.cpp
    inc/AboutDialog.h
    inc/FBIdeApp.h
    inc/FBIdeMainFrame.h
    inc/FormatDialog.h
    inc/InstanceHandler.h
    inc/browser.h
    inc/buffer.h
    inc/bufferlist.h
    inc/configdiag.h
    inc/wxmynotebook.h
    lang.cpp
    loadui.cpp
    run.cpp
    search.cpp
    view.cpp
    wxmynotebook.cpp
    inc/globals.h)

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
