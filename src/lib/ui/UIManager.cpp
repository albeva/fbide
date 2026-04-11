//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "UIManager.hpp"
#include <wx/artprov.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include "MenuId.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"

namespace fbide {

namespace {
    /// Cast MenuId to int for wx APIs.
    constexpr auto id(MenuId mid) -> int { return static_cast<int>(mid); }

    /// Append a menu item with translated label, optional shortcut, and help text.
    void append(wxMenu* menu, const Lang& lang, MenuId mid, LangId label, const wxString& shortcut = "", LangId help = {}) {
        auto text = lang[label];
        if (!shortcut.empty()) {
            text += "\t" + shortcut;
        }
        menu->Append(id(mid), text, lang[help]);
    }

    /// Append a check menu item.
    void appendCheck(wxMenu* menu, const Lang& lang, MenuId mid, LangId label, const wxString& shortcut = "", LangId help = {}) {
        auto text = lang[label];
        if (!shortcut.empty()) {
            text += "\t" + shortcut;
        }
        menu->AppendCheckItem(id(mid), text, lang[help]);
    }
} // namespace

UIManager::UIManager(Context& ctx)
: m_ctx(ctx) {}

void UIManager::createMainFrame() {
    const auto& config = m_ctx.getConfig();

    m_frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
    m_frame->SetEventHandler(this);

    // Position and size from config
    if (config.windowW == -1 || config.windowH == -1) {
        m_frame->Maximize();
    } else {
        m_frame->Move(config.windowX, config.windowY);
        m_frame->SetSize(config.windowW, config.windowH);
    }

    createMenuBar();
    createToolBar();
    createStatusBar();
    createLayout();

    enableEditorMenus(false);

    m_frame->Show();
}

void UIManager::createMenuBar() {
    const auto& lang = m_ctx.getLang();
    const auto menuBar = make_unowned<wxMenuBar>();

    // File menu
    m_fileMenu = make_unowned<wxMenu>();
    append(m_fileMenu, lang, MenuId::New, LangId::FileNew, "Ctrl+N", LangId::FileNewHelp);
    append(m_fileMenu, lang, MenuId::Open, LangId::FileOpen, "Ctrl+O", LangId::FileOpenHelp);
    m_fileMenu->AppendSeparator();
    append(m_fileMenu, lang, MenuId::Save, LangId::FileSave, "Ctrl+S", LangId::FileSaveHelp);
    append(m_fileMenu, lang, MenuId::SaveAs, LangId::FileSaveAs, "Ctrl+Shift+S", LangId::FileSaveAsHelp);
    append(m_fileMenu, lang, MenuId::SaveAll, LangId::FileSaveAll, "", LangId::FileSaveAllHelp);
    m_fileMenu->AppendSeparator();
    append(m_fileMenu, lang, MenuId::SessionLoad, LangId::FileSessionLoad, "", LangId::FileSessionLoadHelp);
    append(m_fileMenu, lang, MenuId::SessionSave, LangId::FileSessionSave, "", LangId::FileSessionSaveHelp);
    m_fileMenu->AppendSeparator();
    append(m_fileMenu, lang, MenuId::Close, LangId::FileClose, "Ctrl+F4", LangId::FileCloseHelp);
    append(m_fileMenu, lang, MenuId::CloseAll, LangId::FileCloseAll, "", LangId::FileCloseAllHelp);
    m_fileMenu->AppendSeparator();
    append(m_fileMenu, lang, MenuId::NewWindow, LangId::FileNewWindow, "Shift+Ctrl+N", LangId::FileNewWindowHelp);
    append(m_fileMenu, lang, MenuId::Quit, LangId::FileQuit, "Ctrl+Q", LangId::FileQuitHelp);

    // Edit menu
    m_editMenu = make_unowned<wxMenu>();
    append(m_editMenu, lang, MenuId::Undo, LangId::EditUndo, "Ctrl+Z", LangId::EditUndoHelp);
    append(m_editMenu, lang, MenuId::Redo, LangId::EditRedo, "Ctrl+Shift+Z", LangId::EditRedoHelp);
    m_editMenu->AppendSeparator();
    append(m_editMenu, lang, MenuId::Cut, LangId::EditCut, "Ctrl+X", LangId::EditCutHelp);
    append(m_editMenu, lang, MenuId::Copy, LangId::EditCopy, "Ctrl+C", LangId::EditCopyHelp);
    append(m_editMenu, lang, MenuId::Paste, LangId::EditPaste, "Ctrl+V", LangId::EditPasteHelp);
    m_editMenu->AppendSeparator();
    append(m_editMenu, lang, MenuId::SelectAll, LangId::EditSelectAll, "Ctrl+A", LangId::EditSelectAllHelp);
    append(m_editMenu, lang, MenuId::SelectLine, LangId::EditSelectLine, "Ctrl+L", LangId::EditSelectLineHelp);
    m_editMenu->AppendSeparator();
    append(m_editMenu, lang, MenuId::IndentIncrease, LangId::EditIndentInc, "Tab", LangId::EditIndentIncHelp);
    append(m_editMenu, lang, MenuId::IndentDecrease, LangId::EditIndentDec, "Shift+Tab", LangId::EditIndentDecHelp);
    m_editMenu->AppendSeparator();
    append(m_editMenu, lang, MenuId::Comment, LangId::EditComment, "Ctrl+M", LangId::EditCommentHelp);
    append(m_editMenu, lang, MenuId::Uncomment, LangId::EditUncomment, "Ctrl+Shift+M", LangId::EditUncommentHelp);

    // Search menu
    m_searchMenu = make_unowned<wxMenu>();
    append(m_searchMenu, lang, MenuId::Find, LangId::SearchFind, "Ctrl+F", LangId::SearchFindHelp);
    append(m_searchMenu, lang, MenuId::FindNext, LangId::SearchFindNext, "F3", LangId::SearchFindNextHelp);
    append(m_searchMenu, lang, MenuId::Replace, LangId::SearchReplace, "Ctrl+R", LangId::SearchReplaceHelp);
    append(m_searchMenu, lang, MenuId::GotoLine, LangId::SearchGotoLine, "Ctrl+G", LangId::SearchGotoLineHelp);

    // View menu
    m_viewMenu = make_unowned<wxMenu>();
    append(m_viewMenu, lang, MenuId::Settings, LangId::ViewSettings, "", LangId::ViewSettingsHelp);
    append(m_viewMenu, lang, MenuId::Format, LangId::ViewFormat, "", LangId::ViewFormatHelp);
    appendCheck(m_viewMenu, lang, MenuId::Result, LangId::ViewResult, "F4", LangId::ViewResultHelp);
    append(m_viewMenu, lang, MenuId::Subs, LangId::ViewSubs, "F2", LangId::ViewSubsHelp);
    append(m_viewMenu, lang, MenuId::CompilerLog, LangId::ViewCompilerLog, "", LangId::ViewCompilerLogHelp);

    // Run menu
    m_runMenu = make_unowned<wxMenu>();
    append(m_runMenu, lang, MenuId::Compile, LangId::RunCompile, "Ctrl+F9", LangId::RunCompileHelp);
    append(m_runMenu, lang, MenuId::CompileAndRun, LangId::RunCompileAndRun, "F9", LangId::RunCompileAndRunHelp);
    append(m_runMenu, lang, MenuId::Run, LangId::RunRun, "Shift+Ctrl+F9", LangId::RunRunHelp);
    append(m_runMenu, lang, MenuId::QuickRun, LangId::RunQuickRun, "F5", LangId::RunQuickRunHelp);
    append(m_runMenu, lang, MenuId::CmdPrompt, LangId::RunCmdPrompt, "F8", LangId::RunCmdPromptHelp);
    append(m_runMenu, lang, MenuId::Parameters, LangId::RunParameters, "", LangId::RunParametersHelp);
    appendCheck(m_runMenu, lang, MenuId::ShowExitCode, LangId::RunShowExitCode, "", LangId::RunShowExitCodeHelp);
    m_runMenu->Check(id(MenuId::ShowExitCode), m_ctx.getConfig().showExitCode);
    appendCheck(m_runMenu, lang, MenuId::ActivePath, LangId::RunActivePath, "", LangId::RunActivePathHelp);
    m_runMenu->Check(id(MenuId::ActivePath), m_ctx.getConfig().activePath);

    // Help menu
    m_helpMenu = make_unowned<wxMenu>();
    m_helpMenu->Append(id(MenuId::Help), lang[LangId::MenuHelp] + "\tF1");
    m_helpMenu->Append(id(MenuId::QuickKeys), "QuickKeys.txt");
    m_helpMenu->Append(id(MenuId::ReadMe), "ReadMe.txt");
    m_helpMenu->AppendSeparator();
    append(m_helpMenu, lang, MenuId::About, LangId::HelpAbout, "", LangId::HelpAboutHelp);

    // Assemble menu bar
    menuBar->Append(m_fileMenu, lang[LangId::MenuFile]);
    menuBar->Append(m_editMenu, lang[LangId::MenuEdit]);
    menuBar->Append(m_searchMenu, lang[LangId::MenuSearch]);
    menuBar->Append(m_viewMenu, lang[LangId::MenuView]);
    menuBar->Append(m_runMenu, lang[LangId::MenuRun]);
    menuBar->Append(m_helpMenu, lang[LangId::MenuHelp]);

    m_frame->SetMenuBar(menuBar);
}

void UIManager::createToolBar() {
    const auto& lang = m_ctx.getLang();
    m_toolbar = m_frame->CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL | wxTB_FLAT);

    auto bmp = [](const wxArtID& artId) {
        return wxArtProvider::GetBitmapBundle(artId, wxART_TOOLBAR);
    };

    m_toolbar->AddTool(id(MenuId::New), lang[LangId::ToolbarNew], bmp(wxART_NEW));
    m_toolbar->AddTool(id(MenuId::Open), lang[LangId::ToolbarOpen], bmp(wxART_FILE_OPEN));
    m_toolbar->AddTool(id(MenuId::Save), lang[LangId::ToolbarSave], bmp(wxART_FILE_SAVE));
    m_toolbar->AddTool(id(MenuId::SaveAll), lang[LangId::ToolbarSaveAll], bmp(wxART_FILE_SAVE_AS));
    m_toolbar->AddTool(id(MenuId::Close), lang[LangId::ToolbarClose], bmp(wxART_CLOSE));
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(id(MenuId::Cut), lang[LangId::ToolbarCut], bmp(wxART_CUT));
    m_toolbar->AddTool(id(MenuId::Copy), lang[LangId::ToolbarCopy], bmp(wxART_COPY));
    m_toolbar->AddTool(id(MenuId::Paste), lang[LangId::ToolbarPaste], bmp(wxART_PASTE));
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(id(MenuId::Undo), lang[LangId::ToolbarUndo], bmp(wxART_UNDO));
    m_toolbar->AddTool(id(MenuId::Redo), lang[LangId::ToolbarRedo], bmp(wxART_REDO));
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(id(MenuId::Compile), lang[LangId::ToolbarCompile], bmp(wxART_EXECUTABLE_FILE));
    m_toolbar->AddTool(id(MenuId::Run), lang[LangId::ToolbarRun], bmp(wxART_GO_FORWARD));
    m_toolbar->AddTool(id(MenuId::CompileAndRun), lang[LangId::ToolbarCompileAndRun], bmp(wxART_GOTO_LAST));
    m_toolbar->AddTool(id(MenuId::QuickRun), lang[LangId::ToolbarQuickRun], bmp(wxART_TIP));
    m_toolbar->AddTool(id(MenuId::Result), lang[LangId::ToolbarResult], bmp(wxART_REPORT_VIEW));

    m_toolbar->Realize();
}

void UIManager::createStatusBar() const {
    const auto& lang = m_ctx.getLang();
    m_frame->CreateStatusBar(2);
    m_frame->SetStatusText(lang[LangId::Welcome]);
}

void UIManager::createLayout() {
    const auto& lang = m_ctx.getLang();

    m_splitter = make_unowned<wxSplitterWindow>(
        m_frame.get(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxSP_3DSASH | wxNO_BORDER
    );
    m_splitter->SetSashGravity(1.0);
    m_splitter->SetMinimumPaneSize(100);

    // Console (error list)
    m_console = make_unowned<wxListCtrl>(
        m_splitter.get(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES
    );
    m_console->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    wxListItem col;
    col.SetAlign(wxLIST_FORMAT_LEFT);

    col.SetText(lang[LangId::ConsoleLine]);
    m_console->InsertColumn(0, col);
    m_console->SetColumnWidth(0, 60);

    col.SetText(lang[LangId::ConsoleFile]);
    m_console->InsertColumn(1, col);
    m_console->SetColumnWidth(1, 150);

    col.SetText(lang[LangId::ConsoleErrorNr]);
    m_console->InsertColumn(2, col);
    m_console->SetColumnWidth(2, 100);

    col.SetText(lang[LangId::ConsoleMessage]);
    m_console->InsertColumn(3, col);
    m_console->SetColumnWidth(3, 600);

    m_console->Hide();

    // Code panel
    m_codePanel = make_unowned<wxPanel>(
        m_splitter.get(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN
    );
    m_codePanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE));

    // Initialize splitter with code panel only (console hidden)
    m_splitter->Initialize(m_codePanel.get());
}

void UIManager::enableEditorMenus(const bool state) const {
    auto* menuBar = m_frame->GetMenuBar();

    constexpr std::array menuItems {
        MenuId::Undo,
        MenuId::Redo,
        MenuId::Cut,
        MenuId::Copy,
        MenuId::Paste,
        MenuId::SelectAll,
        MenuId::Find,
        MenuId::Replace,
        MenuId::Save,
        MenuId::SaveAll,
        MenuId::SaveAs,
        MenuId::Close,
        MenuId::SessionSave,
        MenuId::CloseAll,
        MenuId::SelectLine,
        MenuId::IndentIncrease,
        MenuId::IndentDecrease,
        MenuId::Comment,
        MenuId::Uncomment,
        MenuId::FindNext,
        MenuId::GotoLine,
        MenuId::Format,
        MenuId::Subs,
    };
    for (const auto mid : menuItems) {
        menuBar->Enable(id(mid), state);
    }

    constexpr std::array toolItems {
        MenuId::Save,
        MenuId::SaveAll,
        MenuId::Close,
        MenuId::Cut,
        MenuId::Copy,
        MenuId::Paste,
        MenuId::Undo,
        MenuId::Redo,
    };
    for (const auto mid : toolItems) {
        m_toolbar->EnableTool(id(mid), state);
    }

    constexpr std::array runItems {
        MenuId::Compile,
        MenuId::Run,
        MenuId::CompileAndRun,
        MenuId::QuickRun,
    };
    for (const auto mid : runItems) {
        menuBar->Enable(id(mid), state);
        m_toolbar->EnableTool(id(mid), state);
    }
}

} // namespace fbide
