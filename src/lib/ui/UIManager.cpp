//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "UIManager.hpp"
#include "CompilerLog.hpp"
#include "MenuId.hpp"
#include "lib/app/Context.hpp"
#include "lib/command/CommandManager.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/FileHistory.hpp"
#include "lib/config/Lang.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "rc/icons.hpp"
#ifndef __WXMSW__
namespace XPM {
#include "rc/appicon.xpm"
}
#endif
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(UIManager, wxEvtHandler)
    EVT_CLOSE(UIManager::onClose)
    EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, UIManager::onPageClose)
    EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, UIManager::onPageChanged)
    EVT_AUINOTEBOOK_BG_DCLICK(wxID_ANY, UIManager::onNotebookDblClick)
wxEND_EVENT_TABLE()
// clang-format on

namespace {
/// Cast MenuId to int for wx APIs.
constexpr auto id(MenuId mid) -> int { return static_cast<int>(mid); }

/// Build label with optional shortcut.
auto makeLabel(const Lang& lang, const LangId label, const wxString& shortcut) -> wxString {
    auto text = lang[label];
    if (!shortcut.empty()) {
        text += "\t" + shortcut;
    }
    return text;
}

/// Append a menu item with translated label, optional shortcut, and help text.
void append(
    wxMenu* menu,
    const Lang& lang,
    const MenuId mid,
    const LangId label,
    const wxString& shortcut = "",
    const LangId help = {}
) {
    menu->Append(id(mid), makeLabel(lang, label, shortcut), lang[help]);
}

/// Append a check menu item.
void appendCheck(
    wxMenu* menu,
    const Lang& lang,
    const MenuId mid,
    const LangId label,
    const wxString& shortcut = "",
    const LangId help = {}
) {
    menu->AppendCheckItem(id(mid), makeLabel(lang, label, shortcut), lang[help]);
}
} // namespace

UIManager::UIManager(Context& ctx)
: m_ctx(ctx) {}

UIManager::~UIManager() {
    if (m_frame != nullptr) {
        m_aui.UnInit();
    }
}

void UIManager::onClose(wxCloseEvent& event) {
    // Let DocumentManager handle unsaved documents
    if (!m_ctx.getDocumentManager().prepareToQuit()) {
        event.Veto();
        return;
    }

    // Save window state to config
    auto& config = m_ctx.getConfig();
    if (m_frame->IsMaximized() || m_frame->IsIconized()) {
        config.setWindowW(-1);
        config.setWindowH(-1);
    } else {
        int posX = 0;
        int posY = 0;
        int sizeW = 0;
        int sizeH = 0;
        m_frame->GetPosition(&posX, &posY);
        m_frame->GetSize(&sizeW, &sizeH);
        config.setWindowX(posX);
        config.setWindowY(posY);
        config.setWindowW(sizeW);
        config.setWindowH(sizeH);
    }
    config.save();
    m_ctx.getFileHistory().save();

    // Clean up event handlers before frame destruction
    m_frame->RemoveEventHandler(this);
    m_frame->RemoveEventHandler(&m_ctx.getCommandManager());
    m_frame->Close();
}

void UIManager::createMainFrame() {
    const auto& config = m_ctx.getConfig();

    m_frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
#ifndef __WXMSW__
    m_frame->SetIcon(wxICON(XPM::appicon));
#endif
    m_frame->PushEventHandler(this);
    m_frame->PushEventHandler(&m_ctx.getCommandManager());

    // Position and size from config
    if (config.getWindowW() == -1 || config.getWindowH() == -1) {
        m_frame->Maximize();
    } else {
        m_frame->Move(config.getWindowX(), config.getWindowY());
        m_frame->SetSize(config.getWindowW(), config.getWindowH());
    }

    // Initialize AUI manager
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT);
    m_aui.SetManagedWindow(m_frame);

    createMenuBar();
    createToolBar();
    createStatusBar();
    createLayout();

    applyState(UIState::None);

    m_aui.Update();
    m_frame->Show();
}

void UIManager::onPageClose(wxAuiNotebookEvent& event) {
    // Always veto — DocumentManager handles the actual page deletion
    event.Veto();

    const auto pageIdx = event.GetSelection();
    if (pageIdx == wxNOT_FOUND) {
        return;
    }

    const auto* page = m_notebook->GetPage(static_cast<size_t>(pageIdx));
    auto& docManager = m_ctx.getDocumentManager();
    if (auto* doc = docManager.findByEditor(page)) {
        docManager.closeFile(*doc);
    }
}

void UIManager::onPageChanged(wxAuiNotebookEvent& event) {
    event.Skip();
    const auto sel = event.GetSelection();
    if (sel == wxNOT_FOUND) {
        return;
    }
    m_notebook->GetPage(static_cast<size_t>(sel))->SetFocus();
}

void UIManager::onNotebookDblClick(wxAuiNotebookEvent& event) {
    event.Skip();
    m_ctx.getDocumentManager().newFile();
}

void UIManager::createMenuBar() {
    const auto& lang = m_ctx.getLang();
    const auto menuBar = make_unowned<wxMenuBar>();

    // File menu
    m_fileMenu = make_unowned<wxMenu>();
    append(m_fileMenu, lang, MenuId::New, LangId::FileNew, "Ctrl+N", LangId::FileNewHelp);
    append(m_fileMenu, lang, MenuId::Open, LangId::FileOpen, "Ctrl+O", LangId::FileOpenHelp);
    m_ctx.getFileHistory().getHistory().UseMenu(m_fileMenu);
    m_ctx.getFileHistory().getHistory().AddFilesToMenu();
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
    m_runMenu->Check(id(MenuId::ShowExitCode), m_ctx.getConfig().getShowExitCode());

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

    const auto add = [&](const MenuId menuIdm, const LangId langId, wxBitmap&& bitmap) {
        const auto mask = make_unowned<wxMask>(bitmap, wxColour(192, 192, 192));
        bitmap.SetMask(mask);
        m_toolbar->AddTool(id(menuIdm), lang[langId], std::move(bitmap), lang[langId]);
    };

    // NOLINTBEGIN(*-avoid-c-arrays)
    add(MenuId::New, LangId::ToolbarNew, wxBitmap(XPM::new_xpm));
    add(MenuId::Open, LangId::ToolbarOpen, wxBitmap(XPM::open_xpm));
    add(MenuId::Save, LangId::ToolbarSave, wxBitmap(XPM::save_xpm));
    add(MenuId::SaveAll, LangId::ToolbarSaveAll, wxBitmap(XPM::saveall_xpm));
    add(MenuId::Close, LangId::ToolbarClose, wxBitmap(XPM::close_xpm));
    m_toolbar->AddSeparator();
    add(MenuId::Cut, LangId::ToolbarCut, wxBitmap(XPM::cut_xpm));
    add(MenuId::Copy, LangId::ToolbarCopy, wxBitmap(XPM::copy_xpm));
    add(MenuId::Paste, LangId::ToolbarPaste, wxBitmap(XPM::paste_xpm));
    m_toolbar->AddSeparator();
    add(MenuId::Undo, LangId::ToolbarUndo, wxBitmap(XPM::undo_xpm));
    add(MenuId::Redo, LangId::ToolbarRedo, wxBitmap(XPM::redo_xpm));
    m_toolbar->AddSeparator();
    add(MenuId::Compile, LangId::ToolbarCompile, wxBitmap(XPM::compile_xpm));
    add(MenuId::Run, LangId::ToolbarRun, wxBitmap(XPM::run_xpm));
    add(MenuId::CompileAndRun, LangId::ToolbarCompileAndRun, wxBitmap(XPM::compnrun_xpm));
    add(MenuId::QuickRun, LangId::ToolbarQuickRun, wxBitmap(XPM::qrun_xpm));
    {
        wxBitmap bitmap(XPM::output_xpm);
        const auto mask = make_unowned<wxMask>(bitmap, wxColour(192, 192, 192));
        bitmap.SetMask(mask);
        m_toolbar->AddCheckTool(id(MenuId::Result), lang[LangId::ToolbarResult], std::move(bitmap));
    }
    // NOLINTEND(*-avoid-c-arrays)

    m_toolbar->Realize();
}

void UIManager::createStatusBar() const {
    const auto& lang = m_ctx.getLang();
    m_frame->CreateStatusBar(2);
    m_frame->SetStatusText(lang[LangId::Welcome]);
}

void UIManager::createLayout() {
    // Document notebook (center)
    m_notebook = make_unowned<wxAuiNotebook>(
        m_frame, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_CLOSE_ON_ALL_TABS | wxAUI_NB_MIDDLE_CLICK_CLOSE
    );

    m_aui.AddPane(
        m_notebook.get(),
        wxAuiPaneInfo()
            .Name("notebook")
            .CenterPane()
            .PaneBorder(false)
    );

    // Console / output pane (bottom, hidden by default)
    m_console = make_unowned<OutputConsole>(m_frame.get(), m_ctx);
    m_console->create();

    m_aui.AddPane(
        m_console,
        wxAuiPaneInfo()
            .Name("console")
            .Caption("Output")
            .Bottom()
            .BestSize(-1, 150)
            .Hide()
    );
}

void UIManager::setDocumentState(const UIState state) {
    m_documentState = state;
    applyState(m_compilerState != UIState::None ? m_compilerState : m_documentState);
}

void UIManager::setCompilerState(const UIState state) {
    m_compilerState = state;
    applyState(m_compilerState != UIState::None ? m_compilerState : m_documentState);
}

void UIManager::applyState(const UIState state) const {
    switch (state) {
    case UIState::None: {
        disable(mutableIds);
        break;
    }
    case UIState::FocusedUnknownFile:
        disable(std::array {
            MenuId::Comment,
            MenuId::Uncomment,
            MenuId::Format,
            MenuId::Subs,
            MenuId::Compile,
            MenuId::CompileAndRun,
            MenuId::Run,
            MenuId::QuickRun,
        });
        break;
    case UIState::FocusedValidSourceFile:
        disable(std::array<MenuId, 0> {});
        break;
    case UIState::Compiling:
    case UIState::Running:
        disable(std::array {
            MenuId::Compile,
            MenuId::CompileAndRun,
            MenuId::Run,
            MenuId::QuickRun,
        });
        break;
    }
}

void UIManager::toggleConsole() {
    auto& pane = m_aui.GetPane("console");
    pane.Show(!pane.IsShown());
    m_aui.Update();
    syncConsoleState(pane.IsShown());
}

void UIManager::showConsole() {
    auto& pane = m_aui.GetPane("console");
    if (!pane.IsShown()) {
        pane.Show();
        m_aui.Update();
        syncConsoleState(true);
    }
}

void UIManager::hideConsole() {
    auto& pane = m_aui.GetPane("console");
    if (pane.IsShown()) {
        pane.Hide();
        m_aui.Update();
        syncConsoleState(false);
    }
}

void UIManager::syncConsoleState(const bool visible) const {
    m_viewMenu->Check(id(MenuId::Result), visible);
    m_toolbar->ToggleTool(id(MenuId::Result), visible);
}

auto UIManager::isConsoleVisible() -> bool {
    return m_aui.GetPane("console").IsShown();
}

auto UIManager::getCompilerLog() -> CompilerLog& {
    if (m_compilerLog == nullptr) {
        const auto& lang = m_ctx.getLang();
        m_compilerLog = make_unowned<CompilerLog>(m_frame, lang[LangId::CompilerLogTitle]);
        m_compilerLog->create(m_ctx);
        m_compilerLog->Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) {
            event.Skip();
        });
    }
    return *m_compilerLog;
}

auto UIManager::freeze() -> FreezeLock {
    return FreezeLock { m_frame };
}

void UIManager::disable(const std::ranges::range auto& range) const {
    auto* menuBar = m_frame->GetMenuBar();
    for (const auto menuId : mutableIds) {
        const bool disabled = not std::ranges::contains(range, menuId);
        if (m_toolbar->FindById(id(menuId)) != nullptr) {
            m_toolbar->EnableTool(id(menuId), disabled);
        }
        if (menuBar->FindItem(id(menuId)) != nullptr) {
            menuBar->Enable(id(menuId), disabled);
        }
    }
}

void UIManager::updateEditorSettigs() {
    // Reapply settings to all open editors
    const auto* notebook = getNotebook();
    for (size_t idx = 0; idx < notebook->GetPageCount(); idx++) {
        if (auto* editor = static_cast<Editor*>(notebook->GetPage(idx))) {
            editor->applySettings();
        }
    }
}
