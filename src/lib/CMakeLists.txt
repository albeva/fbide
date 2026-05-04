add_library(fbide_lib)

target_sources(fbide_lib
    PRIVATE
    about/AboutDialog.cpp
    analyses/intellisense/IntellisenseService.cpp
    analyses/lexer/KeywordTables.cpp
    analyses/lexer/MemoryDocument.cpp
    analyses/lexer/StyleLexer.cpp
    analyses/lexer/StyledSource.cpp
    analyses/lexer/VerbatimAnnotator.cpp
    analyses/symbols/SymbolTable.cpp
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
    config/ConfigManager.cpp
    config/FileHistory.cpp
    config/Theme.cpp
    config/Value.cpp
    config/Version.cpp
    document/Document.cpp
    document/DocumentIO.cpp
    document/DocumentManager.cpp
    document/DocumentType.cpp
    document/EncodingDetector.cpp
    document/FileSession.cpp
    document/TextEncoding.cpp
    editor/AutoIndent.cpp
    editor/CodeTransformer.cpp
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
    sidebar/SideBarManager.cpp
    sidebar/SymbolBrowser.cpp
    ui/ArtiProvider.cpp
    ui/CompilerLog.cpp
    ui/EncodingMenu.cpp
    ui/OutputConsole.cpp
    ui/UIManager.cpp
    ui/controls/BBCodeText.cpp
    ui/controls/ColorPicker.cpp
    ui/controls/Panel.cpp
    ui/utilities/FileDropTarget.cpp

    PUBLIC FILE_SET HEADERS FILES
    about/AboutDialog.hpp
    analyses/intellisense/IntellisenseService.hpp
    analyses/lexer/KeywordTables.hpp
    analyses/lexer/MemoryDocument.hpp
    analyses/lexer/StyleLexer.hpp
    analyses/lexer/StyledSource.hpp
    analyses/lexer/Token.hpp
    analyses/lexer/VerbatimAnnotator.hpp
    analyses/symbols/SymbolTable.hpp
    app/App.hpp
    app/Context.hpp
    app/InstanceHandler.hpp
    command/CommandEntry.hpp
    command/CommandId.hpp
    command/CommandManager.hpp
    compiler/AsyncProcess.hpp
    compiler/BuildTask.hpp
    compiler/CompileCommand.hpp
    compiler/CompilerManager.hpp
    compiler/QuoteUtils.hpp
    compiler/RunCommand.hpp
    config/ConfigManager.hpp
    config/FileHistory.hpp
    config/Theme.hpp
    config/ThemeCategory.hpp
    config/Value.hpp
    config/Version.hpp
    document/Document.hpp
    document/DocumentManager.hpp
    document/DocumentType.hpp
    document/EncodingDetector.hpp
    document/FileSession.hpp
    document/TextEncoding.hpp
    editor/AutoIndent.hpp
    editor/CodeTransformer.hpp
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
    settings/panels/SettingsCategory.hpp
    settings/panels/ThemePage.hpp
    sidebar/SideBarManager.hpp
    sidebar/SymbolBrowser.hpp
    ui/ArtiProvider.hpp
    ui/CompilerLog.hpp
    ui/EncodingMenu.hpp
    ui/OutputConsole.hpp
    ui/UIManager.hpp
    ui/UIState.hpp
    ui/controls/BBCodeText.hpp
    ui/controls/ColorPicker.hpp
    ui/controls/Layout.hpp
    ui/controls/Panel.hpp
    ui/utilities/FileDropTarget.hpp
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
target_include_directories(fbide_lib PUBLIC "${CMAKE_BINARY_DIR}/configured_files/include")
target_precompile_headers(fbide_lib PUBLIC pch.hpp)
