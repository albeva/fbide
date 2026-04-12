# fbide library
add_library(fbide_lib)

target_sources(fbide_lib
    PRIVATE
    lib/app/App.cpp
    lib/app/CommandManager.cpp
    lib/app/Context.cpp
    lib/compiler/CompileCommand.cpp
    lib/compiler/CompilerManager.cpp
    lib/config/Config.cpp
    lib/config/FileHistory.cpp
    lib/config/Keywords.cpp
    lib/config/Lang.cpp
    lib/config/Theme.cpp
    lib/editor/Document.cpp
    lib/editor/DocumentManager.cpp
    lib/editor/DocumentType.cpp
    lib/editor/Editor.cpp
    lib/settings/SettingsDialog.cpp
    lib/settings/panels/CompilerPage.cpp
    lib/settings/panels/GeneralPage.cpp
    lib/settings/panels/KeywordsPage.cpp
    lib/settings/panels/Panel.cpp
    lib/settings/panels/ThemePage.cpp
    lib/ui/OutputConsole.cpp
    lib/ui/UIManager.cpp

    PUBLIC FILE_SET HEADERS FILES
    lib/app/App.hpp
    lib/app/CommandManager.hpp
    lib/app/Context.hpp
    lib/compiler/CompileCommand.hpp
    lib/compiler/CompilerManager.hpp
    lib/config/Config.hpp
    lib/config/FileHistory.hpp
    lib/config/Keywords.hpp
    lib/config/Lang.hpp
    lib/config/LangId.hpp
    lib/config/Theme.hpp
    lib/editor/Document.hpp
    lib/editor/DocumentManager.hpp
    lib/editor/DocumentType.hpp
    lib/editor/Editor.hpp
    lib/settings/SettingsDialog.hpp
    lib/settings/panels/CompilerPage.hpp
    lib/settings/panels/GeneralPage.hpp
    lib/settings/panels/KeywordsPage.hpp
    lib/settings/panels/Panel.hpp
    lib/settings/panels/ThemePage.hpp
    lib/ui/MenuId.hpp
    lib/ui/OutputConsole.hpp
    lib/ui/UIManager.hpp
    lib/utils/NoCopy.hpp
    lib/utils/ValueRestorer.hpp
    lib/utils/Macros.hpp
    lib/utils/Unowned.hpp
)

# target settings
target_link_libraries(fbide_lib
    PRIVATE compiler_options compiler_warnings
    PUBLIC ${wxWidgets_LIBRARIES}
)

# precompile header
target_precompile_headers(fbide_lib PUBLIC pch.hpp)

# fbide executable
add_executable(fbide WIN32 main.cpp)
target_link_libraries(fbide PRIVATE fbide_lib compiler_options compiler_warnings)
target_precompile_headers(fbide REUSE_FROM fbide_lib)
set_target_properties(fbide PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
