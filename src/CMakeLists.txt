#
# fbide executable
#
add_executable(fbide WIN32
    pch.h
    Utils.hpp
    App/App.cpp
    App/App.hpp
    App/Manager.cpp
    App/Manager.hpp
    Config/Config.cpp
    Config/Config.hpp
    Config/ConfigManager.cpp
    Config/ConfigManager.hpp
    Config/StyleEntry.cpp
    Config/StyleEntry.hpp
    Document/Document.cpp
    Document/Document.hpp
    Document/DocumentManager.cpp
    Document/DocumentManager.hpp
    Document/TypeManager.cpp
    Document/TypeManager.hpp
    Editor/TextDocument.cpp
    Editor/TextDocument.hpp
    FB/Defaults.hpp
    FB/FBEditor.cpp
    FB/FBEditor.hpp
    FB/Parser/SourceLexer.cpp
    FB/Parser/SourceLexer.hpp
    Log/LogManager.cpp
    Log/LogManager.hpp
    UI/CmdManager.cpp
    UI/CmdManager.hpp
    UI/IArtProvider.cpp
    UI/IArtProvider.hpp
    UI/MenuHandler.cpp
    UI/MenuHandler.hpp
    UI/PanelHandler.cpp
    UI/PanelHandler.hpp
    UI/StandardArtProvider.cpp
    UI/StandardArtProvider.hpp
    UI/ToolbarHandler.cpp
    UI/ToolbarHandler.hpp
    UI/UiManager.cpp
    UI/UiManager.hpp
    FB/Parser/SymbolTable.cpp
    FB/Parser/SymbolTable.hpp
    FB/Parser/Token.cpp
    FB/Parser/Token.hpp)

# Dependencies
add_dependencies(fbide wxstc)

# PCH file
target_precompile_headers(fbide PRIVATE pch.h)

# emit exe to bin/
set_target_properties(fbide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# set src root for includes
target_include_directories(fbide PRIVATE
    ${PROJECT_SOURCE_DIR}/wxSTC/include
    ${fbide_SOURCE_DIR}/src
    )

target_compile_definitions(fbide PRIVATE -DEXPORT_LEXER_IFACE)

# wxWidgets
include(${wxWidgets_USE_FILE})

# link dependencies
target_link_libraries(fbide
    PRIVATE
    ${wxWidgets_LIBRARIES}
    yaml-cpp wxstc
    project_options project_warnings)
