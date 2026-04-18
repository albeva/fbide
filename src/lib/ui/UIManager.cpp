//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "UIManager.hpp"
#include "app/Context.hpp"
#include "CompilerLog.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/DocumentManager.hpp"
#include "editor/Editor.hpp"
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
    auto window = m_ctx.getConfigManager().config()["window"];
    if (m_frame->IsMaximized() || m_frame->IsIconized()) {
        window["width"] = -1;
        window["height"] = -1;
    } else {
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
    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
    m_ctx.getFileHistory().save();

    // Clean up event handlers before frame destruction
    m_frame->RemoveEventHandler(this);
    m_frame->RemoveEventHandler(&m_ctx.getCommandManager());
    m_frame->Close();
}

void UIManager::createMainFrame() {
    m_frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
#ifndef __WXMSW__
    m_frame->SetIcon(wxICON(XPM::appicon));
#endif
    m_frame->PushEventHandler(this);
    m_frame->PushEventHandler(&m_ctx.getCommandManager());

    // Position and size from config
    const auto window = m_ctx.getConfigManager().config().at("window");
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

    // Initialize AUI manager
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT);
    m_aui.SetManagedWindow(m_frame);

    configureMenuBar();
    configureToolBar();
    createStatusBar();
    createLayout();
    m_ctx.getCommandManager().initializeCommands();

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

void UIManager::configureMenuBar() {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& menus = find(cfg.layout().raw(), "menus").as_array();
        const auto& locale = cfg.locale().raw()["menus"];

        const bool createMenus = m_frame->GetMenuBar() == nullptr;
        const auto menuBar = createMenus ? make_unowned<wxMenuBar>() : m_frame->GetMenuBar();

        for (const auto& id : menus) {
            const auto key = id.as_string();
            auto* entry = cmd.find("menus." + key);
            if (entry == nullptr) {
                wxLogError("Unknown menu '%s'", key);
                continue;
            }
            const auto name = find(locale, key).as_string();
            auto menu = entry->get<wxMenu>();
            if (menu == nullptr) {
                menu = make_unowned<wxMenu>();
                entry->binds.push_back(menu);
                menuBar->Append(menu, name);
            } else {
                menu->SetTitle(name);
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
            menuBar->UpdateMenus();
        }
    } catch (const std::exception& ex) {
        wxLogError("Invalid layout config for menus: %s", ex.what());
    }
}

void UIManager::configureMenuItems(wxMenu* menu, const wxString& id, const bool addSeparators) {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& items = find(cfg.layout().raw(), "menu", id).as_array();
        const auto& commands = find(cfg.locale().raw(), "commands");
        const auto& shortcuts = find(cfg.shortcuts().raw(), "commands");

        for (const auto& item : items) {
            const auto key = item.as_string();
            if (key == "-") {
                if (addSeparators) {
                    menu->AppendSeparator();
                }
                continue;
            }

            auto* entry = cmd.find(key);
            if (entry == nullptr) {
                wxLogError("Unknown command '%s'", key);
                continue;
            }

            const auto& locale = find(commands, key);
            auto name = locale.at("name").as_string();
            const auto help = find_or(locale, "help", "");

            const auto shortcut = find_or(shortcuts, key, "");
            if (not shortcut.empty()) {
                name += "\t" + shortcut;
            }

            if (auto* tool = entry->get<wxMenuItem>()) {
                tool->SetItemLabel(name);
                tool->SetHelp(help);
            } else {
                wxMenu* submenu = nullptr;
                if (entry->kind == wxITEM_DROPDOWN) {
                    submenu = entry->get<wxMenu>();
                    if (submenu == nullptr) {
                        submenu = make_unowned<wxMenu>();
                        entry->binds.push_back(submenu);
                    }
                }
                tool = make_unowned<wxMenuItem>(menu, entry->id, name, help, entry->kind, submenu);
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

void UIManager::configureToolBar() {
    try {
        auto& cmd = m_ctx.getCommandManager();
        auto& cfg = m_ctx.getConfigManager();
        const auto& items = find(cfg.layout().raw(), "toolbar").as_array();
        const auto& commands = find(cfg.locale().raw(), "commands");

        // NOLINTBEGIN(*-avoid-c-arrays)
        static const std::unordered_map<wxString, const char* const*> icons = {
            { "new",           XPM::new_xpm      },
            { "open",          XPM::open_xpm     },
            { "save",          XPM::save_xpm     },
            { "saveAll",       XPM::saveall_xpm  },
            { "close",         XPM::close_xpm    },
            { "cut",           XPM::cut_xpm      },
            { "copy",          XPM::copy_xpm     },
            { "paste",         XPM::paste_xpm    },
            { "undo",          XPM::undo_xpm     },
            { "redo",          XPM::redo_xpm     },
            { "compile",       XPM::compile_xpm  },
            { "run",           XPM::run_xpm      },
            { "compileAndRun", XPM::compnrun_xpm },
            { "quickRun",      XPM::qrun_xpm     },
            { "viewResult",    XPM::output_xpm   },
        };
        // NOLINTEND(*-avoid-c-arrays)

        const bool createTools = m_toolbar == nullptr;
        if (createTools) {
            m_toolbar = m_frame->CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL | wxTB_FLAT);
        }

        for (const auto& item : items) {
            const auto key = item.as_string();
            if (key == "-") {
                if (createTools) {
                    m_toolbar->AddSeparator();
                }
                continue;
            }

            auto* entry = cmd.find(key);
            if (entry == nullptr) {
                wxLogError("Unknown command '%s'", key);
                continue;
            }

            const auto& locale = find(commands, key);
            const auto name = locale.at("name").as_string();
            const auto help = find_or(locale, "help", "");

            if (entry->get<wxToolBarToolBase>() != nullptr) {
                m_toolbar->SetToolShortHelp(entry->id, name);
                m_toolbar->SetToolLongHelp(entry->id, help);
                continue;
            }

            const auto iconIt = icons.find(key);
            if (iconIt == icons.end()) {
                wxLogError("No toolbar icon for command '%s'", key);
                continue;
            }

            wxBitmap bitmap(iconIt->second);
            const auto mask = make_unowned<wxMask>(bitmap, wxColour(192, 192, 192));
            bitmap.SetMask(mask);
            auto* tool = m_toolbar->AddTool(entry->id, name, bitmap, help, entry->kind);
            entry->binds.push_back(tool);
        }

        if (createTools) {
            m_toolbar->Realize();
        }
    } catch (const std::exception& ex) {
        wxLogError("Invalid layout config for toolbar: %s", ex.what());
    }
}

void UIManager::createStatusBar() const {
    m_frame->CreateStatusBar(2);
    m_frame->SetStatusText(m_ctx.tr("common.welcome"));
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

    auto* entry = m_ctx.getCommandManager().find(+CommandId::Result);
    if (entry == nullptr) {
        wxLogError("Entry is missing for the result console");
        return;
    }
    m_aui.AddPane(
        m_console,
        wxAuiPaneInfo()
            .Name(entry->name)
            .Caption("Output") // TODO: grab from locale
            .Bottom()
            .BestSize(-1, 150)
            .Hide()
    );

    // This is safe, wx stores panes as heap allocated objects.
    entry->binds.push_back(&m_aui);
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
            CommandId::Comment,
            CommandId::Uncomment,
            CommandId::Format,
            CommandId::Subs,
            CommandId::Compile,
            CommandId::CompileAndRun,
            CommandId::Run,
            CommandId::QuickRun,
        });
        break;
    case UIState::FocusedValidSourceFile:
        disable(std::array<CommandId, 0> {});
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
        item->setChecked(visible);
        m_toolbar->Realize();
    }
}

auto UIManager::getCompilerLog() -> CompilerLog& {
    if (m_compilerLog == nullptr) {
        m_compilerLog = make_unowned<CompilerLog>(m_frame, m_ctx.tr("dialogs.log.title"));
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
        if (m_toolbar->FindById(+menuId) != nullptr) {
            m_toolbar->EnableTool(+menuId, disabled);
        }
        if (menuBar->FindItem(+menuId) != nullptr) {
            menuBar->Enable(+menuId, disabled);
        }
    }
}

void UIManager::refreshUi() {
    configureMenuBar();
    configureToolBar();
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
