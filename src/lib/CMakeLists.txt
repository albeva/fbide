add_library(fbide_lib)

target_sources(fbide_lib
    PRIVATE
    about/AboutDialog.cpp
    analyses/intellisense/IntellisenseService.cpp
    analyses/intellisense/SourceGraph.cpp
    analyses/lexer/KeywordTables.cpp
    analyses/lexer/MemoryDocument.cpp
    analyses/lexer/StyleLexer.cpp
    analyses/lexer/StyledSource.cpp
    analyses/lexer/VerbatimAnnotator.cpp
    analyses/parser/TreeBuilder.cpp
    analyses/parser/TreeParser.cpp
    analyses/symbols/PpConditional.cpp
    analyses/symbols/SymbolTable.cpp
    app/App.cpp
    app/Context.cpp
    app/CrashHandler.cpp
    app/FileAssociations.cpp
    app/FileAssociationsLinux.cpp
    app/InstanceHandler.cpp
    command/CommandEntry.cpp
    command/CommandManager.cpp
    compiler/AsyncProcess.cpp
    compiler/BuildTask.cpp
    compiler/CompileCommand.cpp
    compiler/FbcDefines.cpp
    compiler/CompilerConfigCatalog.cpp
    compiler/CompilerManager.cpp
    compiler/FbcAutoDetect.cpp
    compiler/RunCommand.cpp
    config/ConfigManager.cpp
    config/ConfigStrategy.cpp
    config/FileHistory.cpp
    config/Theme.cpp
    config/Value.cpp
    config/Version.cpp
    document/Document.cpp
    document/DocumentIO.cpp
    document/DocumentInfoBar.cpp
    document/DocumentManager.cpp
    document/DocumentPath.cpp
    document/DocumentType.cpp
    document/DocumentWatcher.cpp
    document/EncodingDetector.cpp
    document/FileSession.cpp
    document/TextEncoding.cpp
    editor/AutoIndent.cpp
    editor/CodeTransformer.cpp
    editor/Editor.cpp
    editor/LineHistory.cpp
    editor/lexilla/FBSciLexer.cpp
    format/FormatCommand.cpp
    format/FormatDialog.cpp
    format/renderers/HtmlRenderer.cpp
    format/renderers/PlainTextRenderer.cpp
    format/renderers/Renderer.cpp
    format/transformers/case/CaseTransform.cpp
    format/transformers/reformat/ReFormatter.cpp
    format/transformers/reformat/Renderer.cpp
    help/HelpManager.cpp
    markdown/CodeFence.cpp
    markdown/CodeHighlighter.cpp
    markdown/Markdown.cpp
    markdown/MarkdownDocument.cpp
    markdown/MarkdownImageCache.cpp
    markdown/MarkdownLayout.cpp
    markdown/MarkdownRenderer.cpp
    markdown/MarkdownView.cpp
    settings/SettingsDialog.cpp
    settings/panels/CompilerPage.cpp
    settings/panels/GeneralPage.cpp
    settings/panels/KeywordsPage.cpp
    settings/panels/ThemePage.cpp
    sidebar/FileBrowser.cpp
    sidebar/FocusableDirCtrl.cpp
    sidebar/SideBarManager.cpp
    sidebar/SymbolBrowser.cpp
    sidebar/SymbolBrowserPanel.cpp
    ui/ArtiProvider.cpp
    ui/CompilerLog.cpp
    ui/DocumentTypeMenu.cpp
    ui/EncodingMenu.cpp
    ui/OutputConsole.cpp
    ui/StatusBarHandler.cpp
    ui/UIManager.cpp
    ui/controls/ColorPicker.cpp
    ui/controls/FlatButton.cpp
    ui/controls/InheritableField.cpp
    ui/controls/Panel.cpp
    ui/controls/SmartBoxSizer.cpp
    ui/utilities/FileDropTarget.cpp
    ui/utilities/SystemShell.cpp
    update/UpdateManager.cpp
    utils/ConsoleOutput.cpp

    PUBLIC FILE_SET HEADERS FILES
    about/AboutDialog.hpp
    analyses/intellisense/IntellisenseService.hpp
    analyses/intellisense/SourceGraph.hpp
    analyses/lexer/KeywordTables.hpp
    analyses/lexer/MemoryDocument.hpp
    analyses/lexer/StyleLexer.hpp
    analyses/lexer/StyledSource.hpp
    analyses/lexer/Token.hpp
    analyses/lexer/VerbatimAnnotator.hpp
    analyses/parser/ProgramTree.hpp
    analyses/parser/TreeBuilder.hpp
    analyses/parser/TreeParser.hpp
    analyses/symbols/PpConditional.hpp
    analyses/symbols/SymbolTable.hpp
    app/App.hpp
    app/Context.hpp
    app/CrashHandler.hpp
    app/InstanceHandler.hpp
    command/CommandEntry.hpp
    command/CommandId.hpp
    command/CommandManager.hpp
    compiler/AsyncProcess.hpp
    compiler/BuildTask.hpp
    compiler/CompileCommand.hpp
    compiler/FbcDefines.hpp
    compiler/CompilerConfigCatalog.hpp
    compiler/CompilerManager.hpp
    compiler/FbcAutoDetect.hpp
    compiler/QuoteUtils.hpp
    compiler/RunCommand.hpp
    config/ConfigManager.hpp
    config/ConfigStrategy.hpp
    config/FileHistory.hpp
    config/Theme.hpp
    config/ThemeCategory.hpp
    config/Value.hpp
    config/Version.hpp
    document/Document.hpp
    document/DocumentManager.hpp
    document/DocumentPath.hpp
    document/DocumentType.hpp
    document/EncodingDetector.hpp
    document/FileSession.hpp
    document/TextEncoding.hpp
    editor/AutoIndent.hpp
    editor/CodeTransformer.hpp
    editor/Editor.hpp
    editor/LineHistory.hpp
    editor/lexilla/CharCategory.hpp
    editor/lexilla/FBSciLexer.hpp
    format/FormatCommand.hpp
    format/FormatDialog.hpp
    format/renderers/HtmlRenderer.hpp
    format/renderers/PlainTextRenderer.hpp
    format/renderers/Renderer.hpp
    format/transformers/Transform.hpp
    format/transformers/case/CaseTransform.hpp
    format/transformers/reformat/FormatOptions.hpp
    format/transformers/reformat/ReFormatter.hpp
    format/transformers/reformat/Renderer.hpp
    help/HelpManager.hpp
    markdown/CodeFence.hpp
    markdown/CodeHighlighter.hpp
    markdown/Markdown.hpp
    markdown/MarkdownDocument.hpp
    markdown/MarkdownImageCache.hpp
    markdown/MarkdownLayout.hpp
    markdown/MarkdownRenderer.hpp
    markdown/MarkdownView.hpp
    settings/SettingsDialog.hpp
    settings/panels/CompilerPage.hpp
    settings/panels/GeneralPage.hpp
    settings/panels/KeywordsPage.hpp
    settings/panels/SettingsCategory.hpp
    settings/panels/ThemePage.hpp
    sidebar/FileBrowser.hpp
    sidebar/FocusableDirCtrl.hpp
    sidebar/MacFileIcons.hpp
    sidebar/SideBarManager.hpp
    sidebar/SymbolBrowser.hpp
    sidebar/SymbolBrowserPanel.hpp
    ui/ArtiProvider.hpp
    ui/CompilerLog.hpp
    ui/DocumentTypeMenu.hpp
    ui/EncodingMenu.hpp
    ui/OutputConsole.hpp
    ui/StatusBarHandler.hpp
    ui/UIManager.hpp
    ui/UIState.hpp
    ui/controls/ColorPicker.hpp
    ui/controls/FlatButton.hpp
    ui/controls/InheritableField.hpp
    ui/controls/Layout.hpp
    ui/controls/Panel.hpp
    ui/controls/SmartBoxSizer.hpp
    ui/utilities/FileDropTarget.hpp
    ui/utilities/SystemShell.hpp
    update/UpdateManager.hpp
    utils/ConsoleOutput.hpp
    utils/DeferHandler.hpp
    utils/Macros.hpp
    utils/NoCopy.hpp
    utils/PathConversions.hpp
    utils/Unowned.hpp
    utils/Utils.hpp
    utils/ValueRestorer.hpp
)

# target settings
target_link_libraries(fbide_lib
    PRIVATE compiler_options compiler_warnings
)
linkWxWidgets(fbide_lib)

# macOS: native file-browser icons (NSWorkspace). Obj-C++ source, compiled
# without the C++ PCH and with ARC; needs the UniformTypeIdentifiers framework
# (AppKit comes via wxWidgets, but list it for clarity).
if (APPLE)
    target_sources(fbide_lib PRIVATE sidebar/MacFileIcons.mm)
    set_source_files_properties(sidebar/MacFileIcons.mm PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-fobjc-arc"
    )
    target_link_libraries(fbide_lib PUBLIC "-framework AppKit" "-framework UniformTypeIdentifiers")
endif ()

# nlohmann/json — header-only; added as a SYSTEM include so its headers are
# exempt from the project's -Werror / /WX warning level.
target_include_directories(fbide_lib SYSTEM PRIVATE
    "${nlohmann_json_SOURCE_DIR}/include"
)

# md4c Markdown parser (static lib; propagates its SYSTEM include dir).
target_link_libraries(fbide_lib PRIVATE md4c)

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
# Generated image assets (resources/scripts/make-doc-icons.py): the #include'd
# appicon.xpm, plus app.rc's fbide.ico. PUBLIC so consumers (the fbide exe's RC,
# tests) get it too.
target_include_directories(fbide_lib PUBLIC "${PROJECT_SOURCE_DIR}/resources/images")
target_precompile_headers(fbide_lib PUBLIC pch.hpp)
