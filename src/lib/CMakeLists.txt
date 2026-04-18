add_library(fbide_lib)

target_sources(fbide_lib
    PRIVATE
    about/AboutDialog.cpp
    analyses/lexer/Lexer.cpp
    app/App.cpp
    app/Context.cpp
    app/InstanceHandler.cpp
    command/CommandEntry.cpp
    command/CommandManager.cpp
    compiler/AsyncProcess.cpp
    compiler/BuildTask.cpp
    compiler/CompileCommand.cpp
    compiler/CompilerManager.cpp
    compiler/RunCommand.cpp
    config/Config.cpp
    config/ConfigManager.cpp
    config/FileHistory.cpp
    config/Keywords.cpp
    config/Lang.cpp
    config/Theme.cpp
    config/Value.cpp
    editor/Document.cpp
    editor/DocumentManager.cpp
    editor/DocumentType.cpp
    editor/Editor.cpp
    editor/lexilla/FBSciLexer.cpp
    format/FormatDialog.cpp
    format/renderers/HtmlRenderer.cpp
    format/renderers/PlainTextRenderer.cpp
    format/renderers/Renderer.cpp
    format/transformers/case/CaseTransform.cpp
    format/transformers/reformat/ReFormatter.cpp
    format/transformers/reformat/Renderer.cpp
    format/transformers/reformat/TreeBuilder.cpp
    help/HelpManager.cpp
    settings/SettingsDialog.cpp
    settings/panels/CompilerPage.cpp
    settings/panels/GeneralPage.cpp
    settings/panels/KeywordsPage.cpp
    settings/panels/ThemePage.cpp
    ui/BBCodeText.cpp
    ui/CompilerLog.cpp
    ui/OutputConsole.cpp
    ui/Panel.cpp
    ui/UIManager.cpp

    PUBLIC FILE_SET HEADERS FILES
    about/AboutDialog.hpp
    analyses/lexer/Lexer.hpp
    analyses/lexer/Token.hpp
    app/App.hpp
    app/Context.hpp
    app/InstanceHandler.hpp
    command/CommandEntry.hpp
    command/CommandManager.hpp
    compiler/AsyncProcess.hpp
    compiler/BuildTask.hpp
    compiler/CompileCommand.hpp
    compiler/CompilerManager.hpp
    compiler/QuoteUtils.hpp
    compiler/RunCommand.hpp
    config/Config.hpp
    config/ConfigManager.hpp
    config/FileHistory.hpp
    config/Keywords.hpp
    config/Lang.hpp
    config/LangId.hpp
    config/Theme.hpp
    config/Value.hpp
    editor/Document.hpp
    editor/DocumentManager.hpp
    editor/DocumentType.hpp
    editor/Editor.hpp
    editor/lexilla/CharCategory.hpp
    editor/lexilla/FBSciLexer.hpp
    format/FormatDialog.hpp
    format/renderers/HtmlRenderer.hpp
    format/renderers/PlainTextRenderer.hpp
    format/renderers/Renderer.hpp
    format/transformers/Transform.hpp
    format/transformers/case/CaseTransform.hpp
    format/transformers/reformat/FormatTree.hpp
    format/transformers/reformat/ReFormatter.hpp
    format/transformers/reformat/Renderer.hpp
    format/transformers/reformat/TreeBuilder.hpp
    help/HelpManager.hpp
    settings/SettingsDialog.hpp
    settings/panels/CompilerPage.hpp
    settings/panels/GeneralPage.hpp
    settings/panels/KeywordsPage.hpp
    settings/panels/ThemePage.hpp
    ui/BBCodeText.hpp
    ui/CompilerLog.hpp
    ui/Layout.hpp
    ui/MenuId.hpp
    ui/OutputConsole.hpp
    ui/Panel.hpp
    ui/UIManager.hpp
    ui/UIState.hpp
    utils/DeferHandler.hpp
    utils/Macros.hpp
    utils/NoCopy.hpp
    utils/Unowned.hpp
    utils/Utils.hpp
    utils/ValueRestorer.hpp
)

# target settings
target_link_libraries(fbide_lib
    PRIVATE compiler_options compiler_warnings
    PUBLIC toml11::toml11
)
linkWxWidgets(fbide_lib)

# Scintilla/Lexilla for custom lexer
if (TARGET lexilla_lexlib)
    # Non-Windows: link lexlib static library (provides include dirs transitively)
    target_link_libraries(fbide_lib PRIVATE lexilla_lexlib)
else ()
    # Windows (static wxWidgets): symbols come from static libs, just need headers
    target_include_directories(fbide_lib SYSTEM PRIVATE
        "${PROJECT_SOURCE_DIR}/third_party/scintilla/include"
        "${PROJECT_SOURCE_DIR}/third_party/lexilla/include"
        "${PROJECT_SOURCE_DIR}/third_party/lexilla/lexlib"
    )
endif ()

# precompile header
target_include_directories(fbide_lib PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
target_precompile_headers(fbide_lib PUBLIC pch.hpp)
