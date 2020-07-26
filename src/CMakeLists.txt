#
# fbide executable
#
add_executable(fbide WIN32
    app_pch.hpp
    Utils.hpp
    App/App.cpp
    App/Manager.hpp
    App/Manager.cpp
    UI/CmdManager.hpp
    UI/CmdManager.cpp
    UI/IArtProvider.hpp
    UI/MainWindow.hpp
    UI/MainWindow.cpp
    UI/MenuHandler.hpp
    UI/MenuHandler.cpp
    UI/StandardArtProvider.hpp
    UI/StandardArtProvider.cpp
    UI/ToolbarHandler.hpp
    UI/ToolbarHandler.cpp
    UI/UiManager.hpp
    UI/UiManager.cpp
    Config/Config.hpp
    Config/Config.cpp
    Config/ConfigManager.hpp
    Config/ConfigManager.cpp
    Editor/Document.hpp
    Editor/Document.cpp
    Editor/EditorDocument.hpp
    Editor/EditorDocument.cpp
    Editor/StyledEditor.hpp
    Editor/StyledEditor.cpp
    Editor/TypeManager.hpp
    Editor/TypeManager.cpp
    )

# Dependencies
add_dependencies(fbide fblexer yaml-cpp)

# PCH file
target_precompile_headers(fbide INTERFACE app_pch.hpp)

# emit exe to bin/
set_target_properties(fbide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# set src root for includes
target_include_directories(fbide PRIVATE
    ${fbide_SOURCE_DIR}/src)

# wxWidgets
include(${wxWidgets_USE_FILE})

# link dependencies
target_link_libraries(fbide
    PRIVATE
    ${wxWidgets_LIBRARIES}
    yaml-cpp)