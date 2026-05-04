//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "UIManager.hpp"
#include "CompilerLog.hpp"
#include "EncodingMenu.hpp"
#include "app/Context.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "rc/icons.hpp"
#include "sidebar/SideBarManager.hpp"
#ifndef __WXMSW__
namespace XPM {
#include "rc/appicon.xpm"
}
#endif
using namespace fbide;

namespace {
const int DocumentTabsId = wxNewId();
}

// clang-format off
wxBEGIN_EVENT_TABLE(UIManager, wxEvtHandler)
    EVT_CLOSE(UIManager::onClose)
    EVT_AUINOTEBOOK_PAGE_CLOSE(DocumentTabsId,   UIManager::onPageClose)
    EVT_AUINOTEBOOK_PAGE_CHANGED(DocumentTabsId, UIManager::onPageChanged)
    EVT_AUINOTEBOOK_BG_DCLICK(DocumentTabsId,    UIManager::onNotebookDblClick)
wxEND_EVENT_TABLE()
// clang-format on

UIManager::UIManager(Context& ctx)
: m_ctx(ctx)
, m_artProvider(std::make_unique<ArtiProvider>()) {}

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

    saveWindowGeometry();
    // Document tabs are gone at this point — the AUI perspective we
    // capture now reflects only persistent chrome (toolbars, sidebar,
    // output console) without any transient document state baked in.
    saveAuiPerspective();
    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
    m_ctx.getFileHistory().save();

    // Clean up event handlers before frame destruction
    m_frame->RemoveEventHandler(this);
    m_frame->RemoveEventHandler(&m_ctx.getCommandManager());
    m_frame->Close();
}

void UIManager::saveWindowGeometry() {
    auto& window = m_ctx.getConfigManager().config()["window"];
    if (m_frame->IsMaximized() || m_frame->IsIconized()) {
        window["width"] = -1;
        window["height"] = -1;
        return;
    }
    int posX = 0;
    int posY = 0;
    int sizeW = 0;
    int sizeH = 0;
    m_frame->GetPosition(&posX, &posY);
    m_frame->GetSize(&sizeW, &sizeH);
    window["x"] = posX;
    window["y"] = posY;
    window["width"] = sizeW;
    window["height"] = sizeH;
}

void UIManager::createMainFrame() {
    m_frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
#ifndef __WXMSW__
    m_frame->SetIcon(wxICON(XPM::appicon));
#endif
    m_frame->PushEventHandler(this);
    m_frame->PushEventHandler(&m_ctx.getCommandManager());

    // Position and size from config
    const auto& window = m_ctx.getConfigManager().config().at("window");
    const int winW = window.get_or("width", wxDefaultSize.GetWidth());
    const int winH = window.get_or("height", wxDefaultSize.GetHeight());
    if (winW == -1 || winH == -1) {
        m_frame->Maximize();
    } else {
        m_frame->Move(
            window.get_or("x", wxDefaultPosition.x),
            window.get_or("y", wxDefaultPosition.y)
        );
        m_frame->SetSize(winW, winH);
    }

    // Initialize AUI manager. wxAuiManager's default art provider was
    // constructed when this UIManager was instantiated — well before
    // App::initAppearance had a chance to set the dark/light mode —
    // so its cached wxSystemSettings palette is stale. Refresh it
    // here, after appearance has been applied. The same applies to
    // any wxAuiNotebook / wxAuiToolBar art created later;
    // refreshAuiArt() walks them all once the layout is built.
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT);
    m_aui.SetManagedWindow(m_frame);
    if (auto* art = m_aui.GetArtProvider()) {
        art->UpdateColoursFromSystem();
    }

    configureMenuBar();
    configureToolBar();
    createStatusBar();
    createLayout();
    m_ctx.getDocumentManager().attachNotebook();
    m_ctx.getCommandManager().initializeCommands();

    refreshAuiArt();

    // Restore the dock layout the user left the IDE in last session.
    // Must happen AFTER every pane has been added (toolbar, notebook,
    // sidebar, console) so pane lookup by name succeeds.
    loadAuiPerspective();

    applyState(UIState::None);
    m_aui.Update();

    // Create the compiler log dialog up-front, hidden. BuildTask
    // populates it as soon as compilation starts, so showing the
    // dialog later only flips visibility — content is already there.
    m_compilerLog = make_unowned<CompilerLog>(m_frame, m_ctx.tr("dialogs.log.title"));
    m_compilerLog->create(m_ctx);
    m_compilerLog->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        event.Veto();
        m_compilerLog->Hide();
    });

    m_frame->Show();
}

void UIManager::saveAuiPerspective() {
    // Called on close, after DocumentManager::prepareToQuit has closed
    // every editor tab — the perspective at this point describes only
    // the persistent chrome (toolbars, sidebar, output console) with
    // no transient document state baked in.
    auto& aui = m_ctx.getConfigManager().config()["aui"];
    aui["perspective"] = m_aui.SavePerspective();
}

void UIManager::loadAuiPerspective() {
    const auto perspective = m_ctx.getConfigManager().config().get_or("aui.perspective", "");
    if (perspective.IsEmpty()) {
        return;
    }
    // update=false: defer the visual refresh — the createMainFrame
    // caller invokes m_aui.Update() once for both the restored layout
    // and any state changes that happened earlier.
    m_aui.LoadPerspective(perspective, false);
}

void UIManager::refreshAuiArt() const {
    // Walk every AUI-managed widget that has a colour-cached art
    // provider and ask it to re-read wxSystemSettings. Keeps light /
    // dark mode in sync across panes after a SetAppearance call.
    if (auto* art = m_aui.GetArtProvider()) {
        art->UpdateColoursFromSystem();
    }
    if (m_notebook != nullptr) {
        if (auto* art = m_notebook->GetArtProvider()) {
            art->UpdateColoursFromSystem();
        }
    }
    if (m_sideBar != nullptr) {
        if (auto* art = m_sideBar->GetArtProvider()) {
            art->UpdateColoursFromSystem();
        }
    }
    if (m_auiToolbar != nullptr) {
        if (auto* art = m_auiToolbar->GetArtProvider()) {
            art->UpdateColoursFromSystem();
        }
    }
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
        m_ctx.getSideBarManager().showSymbolsFor(nullptr);
        return;
    }
    auto* page = m_notebook->GetPage(static_cast<size_t>(sel));
    page->SetFocus();
    const auto* doc = m_ctx.getDocumentManager().findByEditor(page);
    m_ctx.getSideBarManager().showSymbolsFor(doc);
}

void UIManager::onNotebookDblClick(wxAuiNotebookEvent& event) {
    event.Skip();
    m_ctx.getDocumentManager().newFile();
}

void UIManager::configureMenuBar() {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& menus = cfg.layout().at("menus");
        const auto& localeMenus = cfg.locale().at("menus");

        const bool createMenus = m_frame->GetMenuBar() == nullptr;
        const auto menuBar = createMenus ? make_unowned<wxMenuBar>() : m_frame->GetMenuBar();

        for (const auto& key : menus.asArray()) {
            if (key.empty()) {
                continue;
            }
            auto* entry = cmd.find("menus." + key);
            if (entry == nullptr) {
                wxLogError("Unknown menu '%s'", key);
                continue;
            }
            const auto name = localeMenus.get_or(key, "");
            auto menu = entry->get<wxMenu>();
            if (menu == nullptr) {
                menu = make_unowned<wxMenu>();
                entry->binds.push_back(menu);
                menuBar->Append(menu, name);
            } else {
                for (std::size_t idx = 0; idx < menuBar->GetMenuCount(); idx++) {
                    if (menuBar->GetMenu(idx) == menu) {
                        menuBar->SetMenuLabel(idx, name);
                        break;
                    }
                }
            }
            configureMenuItems(menu, key, createMenus);
        }

        if (createMenus) {
            if (auto* menu = cmd.find<wxMenu>(+CommandId::RecentFiles)) {
                auto& history = m_ctx.getFileHistory().getHistory();
                history.UseMenu(menu);
                history.AddFilesToMenu();
            }
            m_frame->SetMenuBar(menuBar);
        } else {
            menuBar->Update();
        }
    } catch (const std::exception& ex) {
        wxLogError("Invalid layout config for menus: %s", ex.what());
    }
}

void UIManager::configureMenuItems(wxMenu* menu, const wxString& id, const bool addSeparators) {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& items = cfg.layout().at("menu").at(id);
        const auto& commands = cfg.locale().at("commands");
        const auto& shortcuts = cfg.shortcuts().at("commands");

        for (const auto& key : items.asArray()) {
            if (key == "-") {
                if (addSeparators) {
                    menu->AppendSeparator();
                }
                continue;
            }

            if (key == "externalLinks") {
                generateExternalLinks(menu);
                continue;
            }

            auto* entry = cmd.find(key);
            if (entry == nullptr) {
                wxLogError("Unknown command '%s'", key);
                continue;
            }

            const auto& locale = commands.at(key);
            auto name = locale.get_or("name", "");
            const auto help = locale.get_or("help", "");

            if (const auto shortcut = shortcuts.get_or(key, ""); not shortcut.empty()) {
                name += "\t" + shortcut;
            }

            if (auto* tool = entry->get<wxMenuItem>()) {
                tool->SetItemLabel(name);
                tool->SetHelp(help);
            } else {
                wxMenu* submenu = nullptr;
                wxItemKind kind = entry->kind;
                if (entry->kind == wxITEM_DROPDOWN) {
                    submenu = entry->get<wxMenu>();
                    if (submenu == nullptr) {
                        submenu = make_unowned<wxMenu>();
                        entry->binds.push_back(submenu);
                    }
                    kind = wxITEM_NORMAL;
                }
                tool = make_unowned<wxMenuItem>(menu, entry->id, name, help, kind, submenu);
                entry->binds.push_back(tool);
                menu->Append(tool);
            }

            // traverse subfolder?
            if (auto* submenu = entry->get<wxMenu>()) {
                configureMenuItems(submenu, entry->name, addSeparators);
            }
        }
    } catch (const std::exception& ex) {
        wxLogError("Failed to configure menu '%s': %s", id, ex.what());
    }
}

void UIManager::generateExternalLinks(wxMenu* menu) {
    auto& cmd = m_ctx.getCommandManager();
    auto& cfg = m_ctx.getConfigManager();

    for (wxMenuItem* item : m_externalLinkItems) {
        if (item != nullptr) {
            menu->Destroy(item);
        }
    }
    m_externalLinkItems.clear();
    cmd.clearExternalLinks();

    const auto& urls = cfg.config().at("help.external");
    if (!urls.isTable()) {
        return;
    }
    const auto& labels = cfg.locale().at("help.external");

    for (const auto& [key, value] : urls.entries()) {
        const auto url = value->value_or(wxString {});
        if (url.empty()) {
            wxLogWarning("External link '%s' has empty URL", key);
            continue;
        }
        const auto id = cmd.registerExternalLink(url);
        if (id == wxID_ANY) {
            wxLogWarning("External link slot exhausted, '%s' skipped", key);
            break;
        }
        const auto label = labels.get_or(key, url);
        wxMenuItem* item = make_unowned<wxMenuItem>(menu, id, label, url);
        menu->Append(item);
        m_externalLinkItems.push_back(item);
    }
}

void UIManager::configureToolBar() {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& items = cfg.layout().at("toolbar");
        const auto& commands = cfg.locale().at("commands");

        // Experimental: route the toolbar through wxAUI when
        // `toolbar.useAui=1` in config. Off by default; not yet exposed
        // in the Settings dialog. See GitHub issue #11.
        const bool useAui = cfg.config().get_or("toolbar.useAui", false);
        const bool createTools = (m_toolbar == nullptr && m_auiToolbar == nullptr);

        if (createTools) {
            if (useAui) {
                m_auiToolbar = make_unowned<wxAuiToolBar>(
                    m_frame, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                    wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_GRIPPER | wxAUI_TB_OVERFLOW
                );
            } else {
                m_toolbar = m_frame->CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL | wxTB_FLAT);
            }
        }

        for (const auto& key : items.asArray()) {
            if (key == "-") {
                if (createTools) {
                    if (m_auiToolbar != nullptr) {
                        m_auiToolbar->AddSeparator();
                    } else {
                        m_toolbar->AddSeparator();
                    }
                }
                continue;
            }

            auto* entry = cmd.find(key);
            if (entry == nullptr) {
                wxLogError("Unknown command '%s'", key);
                continue;
            }

            const auto& locale = commands.at(key);
            const auto name = locale.get_or("name", "");
            const auto help = locale.get_or("help", "");

            // Reconfigure path (locale change etc.) — refresh tooltips,
            // skip re-add.
            if (m_auiToolbar != nullptr && entry->get<wxAuiToolBar>() != nullptr) {
                if (auto* item = m_auiToolbar->FindTool(entry->id)) {
                    item->SetLabel(name);
                    item->SetShortHelp(name);
                    item->SetLongHelp(help);
                }
                continue;
            }
            if (m_toolbar != nullptr && entry->get<wxToolBarToolBase>() != nullptr) {
                m_toolbar->SetToolShortHelp(entry->id, name);
                m_toolbar->SetToolLongHelp(entry->id, help);
                continue;
            }

            const auto bitmap = m_artProvider->getBitmap(static_cast<CommandId>(entry->id));
            if (!bitmap.IsOk()) {
                wxLogWarning("No toolbar icon for command '%s'", key);
                continue;
            }

            if (m_auiToolbar != nullptr) {
                m_auiToolbar->AddTool(entry->id, name, bitmap, help, entry->kind);
                m_auiToolbar->SetToolShortHelp(entry->id, name);
                m_auiToolbar->SetToolLongHelp(entry->id, help);
                entry->binds.push_back(m_auiToolbar.get());
            } else {
                auto* tool = m_toolbar->AddTool(entry->id, name, bitmap, help, entry->kind);
                entry->binds.push_back(tool);
            }
        }

        if (createTools) {
            if (m_auiToolbar != nullptr) {
                m_auiToolbar->Realize();
                m_aui.AddPane(
                    m_auiToolbar.get(),
                    wxAuiPaneInfo()
                        .Name("toolbar")
                        .ToolbarPane()
                        .Top()
                        .Gripper(false)
                        .CaptionVisible(false)
                        .DockFixed(true)
                        .CloseButton(false)
                        .PaneBorder(false)
                );
            } else {
                m_toolbar->Realize();
            }
        }
    } catch (const std::exception& ex) {
        wxLogError("Invalid layout config for toolbar: %s", ex.what());
    }
}

void UIManager::createStatusBar() const {
    auto* bar = m_frame->CreateStatusBar(4);
    // Field 0 = welcome / status message (stretch)
    // Field 1 = line : column
    // Field 2 = EOL mode
    // Field 3 = encoding
    constexpr int widths[] = { -1, 90, 90, 140 };
    bar->SetStatusWidths(4, widths);
    m_frame->SetStatusText(m_ctx.tr("common.welcome"));

    bar->Bind(wxEVT_LEFT_DOWN, &UIManager::onStatusBarClick, const_cast<UIManager*>(this));
}

void UIManager::onStatusBarClick(wxMouseEvent& event) {
    event.Skip();
    auto* bar = m_frame->GetStatusBar();
    if (bar == nullptr) {
        return;
    }
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }

    const auto pos = event.GetPosition();
    wxRect rect;

    if (bar->GetFieldRect(2, rect) && rect.Contains(pos)) {
        const auto menu = EncodingMenu::buildEolMenu(doc->getEolMode());
        menu->Bind(wxEVT_MENU, [doc](const wxCommandEvent& evt) {
            if (const auto mode = EncodingMenu::eolFromId(evt.GetId())) {
                doc->setEolMode(*mode);
                doc->getEditor()->updateStatusBar();
            }
        });
        bar->PopupMenu(menu.get());
        return;
    }

    if (bar->GetFieldRect(3, rect) && rect.Contains(pos)) {
        auto menu = EncodingMenu::buildEncodingMenu(
            doc->getEncoding(),
            m_ctx.tr("statusbar.encoding.reloadWithEncoding")
        );
        menu->Bind(wxEVT_MENU, [this, doc](const wxCommandEvent& evt) {
            if (const auto enc = EncodingMenu::encodingSaveFromId(evt.GetId())) {
                doc->setEncoding(*enc);
                m_ctx.getDocumentManager().updateActiveTabTitle();
                doc->getEditor()->updateStatusBar();
                return;
            }
            if (const auto enc = EncodingMenu::encodingReloadFromId(evt.GetId())) {
                m_ctx.getDocumentManager().reloadWithEncoding(*doc, *enc);
            }
        });
        bar->PopupMenu(menu.get());
    }
}

void UIManager::createLayout() {
    // Document notebook (center)
    m_notebook = make_unowned<wxAuiNotebook>(
        m_frame, DocumentTabsId,
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

    auto* entry = m_ctx.getCommandManager().find(+CommandId::Result);
    if (entry == nullptr) {
        wxLogError("Entry is missing for the result console");
        return;
    }
    m_aui.AddPane(
        m_console,
        wxAuiPaneInfo()
            .Name(entry->name)
            .Caption(m_ctx.tr("panels.results.title"))
            .Bottom()
            .BestSize(-1, 150)
            .Hide()
    );

    // This is safe, wx stores panes as heap allocated objects.
    entry->binds.push_back(&m_aui);

    // Sidebar (Browser) pane (left, hidden by default)
    m_sideBar = make_unowned<wxAuiNotebook>(
        m_frame, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxAUI_NB_TOP | wxAUI_NB_SCROLL_BUTTONS
    );

    auto* browserEntry = m_ctx.getCommandManager().find(+CommandId::Browser);
    if (browserEntry == nullptr) {
        wxLogError("Entry is missing for the browser sidebar");
        return;
    }
    m_aui.AddPane(
        m_sideBar.get(),
        wxAuiPaneInfo()
            .Name(SideBarManager::kBrowserPaneName)
            .Caption(m_ctx.tr("sidebar.title"))
            .Left()
            .BestSize(220, -1)
            .Hide()
    );
    browserEntry->binds.push_back(&m_aui);
    m_ctx.getSideBarManager().attach(m_sideBar.get());
}

void UIManager::setDocumentState(const UIState state) {
    m_documentState = state;
    applyState(m_compilerState != UIState::None ? m_compilerState : m_documentState);
    m_ctx.getDocumentManager().syncEditCommands();
}

void UIManager::setCompilerState(const UIState state) {
    m_compilerState = state;
    applyState(m_compilerState != UIState::None ? m_compilerState : m_documentState);
    m_ctx.getDocumentManager().syncEditCommands();
}

void UIManager::applyState(const UIState state) const {
    switch (state) {
    case UIState::None: {
        disable(mutableIds);
        break;
    }
    case UIState::FocusedUnknownFile:
        disable(std::array {
            CommandId::Comment,
            CommandId::Uncomment,
            CommandId::Format,
            CommandId::Subs,
            CommandId::Compile,
            CommandId::CompileAndRun,
            CommandId::Run,
            CommandId::QuickRun,
            CommandId::KillProcess,
        });
        break;
    case UIState::FocusedValidSourceFile:
        disable(std::array {
            CommandId::KillProcess,
        });
        break;
    case UIState::Compiling:
    case UIState::Running:
        disable(std::array {
            CommandId::Compile,
            CommandId::CompileAndRun,
            CommandId::Run,
            CommandId::QuickRun,
        });
        break;
    }
}

void UIManager::showConsole(const bool show) {
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Result)) {
        entry->setChecked(show);
    }
}

void UIManager::syncConsoleState(const bool visible) const {
    if (auto* item = m_ctx.getCommandManager().find(+CommandId::Result)) {
        // CommandEntry::setChecked walks every bound control (menu,
        // toolbar tool, AUI toolbar tool) — no toolbar-specific
        // Realize / Refresh poke needed here any more.
        item->setChecked(visible);
    }
}

auto UIManager::getCompilerLog() -> CompilerLog& {
    return *m_compilerLog;
}

auto UIManager::freeze() -> FreezeLock {
    return FreezeLock { m_frame };
}

void UIManager::disable(const std::ranges::range auto& range) const {
    // Route every state change through CommandEntry. The entry's
    // visitor handles each bound control type (menu item, classic
    // wxToolBar tool, wxAuiToolBar tool), so this loop doesn't care
    // which toolbar flavour is active.
    //
    // Semantics (preserved from the previous direct-poke version):
    // commands listed in `range` get disabled, every other mutable id
    // is left enabled.
    auto& cmd = m_ctx.getCommandManager();
    for (const auto menuId : mutableIds) {
        auto* entry = cmd.find(+menuId);
        if (entry == nullptr) {
            continue;
        }
        entry->setEnabled(!std::ranges::contains(range, menuId));
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
