//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentNotebook.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "DocumentPath.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "editor/Editor.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
/// Matches the style flags `UIManager` historically used when it built
/// the document notebook in `createLayout()`. Centralised here so the
/// constant lives next to the class that owns the widget — and so any
/// future tweak (e.g. dropping `wxAUI_NB_TAB_SPLIT`) is a one-line
/// change with a clear owner.
constexpr long kNotebookStyle = wxAUI_NB_TOP
                              | wxAUI_NB_TAB_SPLIT
                              | wxAUI_NB_TAB_MOVE
                              | wxAUI_NB_SCROLL_BUTTONS
                              | wxAUI_NB_CLOSE_ON_ALL_TABS
                              | wxAUI_NB_MIDDLE_CLICK_CLOSE;

/// Right-click context-menu IDs. Local to the notebook because they
/// only make sense inside the per-tab menu. `constexpr int` rather
/// than `wxNewId()` at static-init time (which trips
/// `bugprone-throwing-static-initialization`) or a scoped enum
/// (which would force a cast at every `wxMenu` call site). The base
/// is past `wxID_HIGHEST`.
constexpr int kTabCloseOthersId = wxID_HIGHEST + 100;
constexpr int kTabShowInBrowserId = wxID_HIGHEST + 101;
constexpr int kTabReloadFromDiskId = wxID_HIGHEST + 102;
const int kNotebookId = ::wxNewId();
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(DocumentNotebook, wxAuiNotebook)
    EVT_AUINOTEBOOK_PAGE_CLOSE(kNotebookId,     DocumentNotebook::onPageClose)
    EVT_AUINOTEBOOK_PAGE_CHANGED(kNotebookId,   DocumentNotebook::onPageChanged)
    EVT_AUINOTEBOOK_BG_DCLICK(kNotebookId,      DocumentNotebook::onBgDClick)
    EVT_AUINOTEBOOK_TAB_RIGHT_DOWN(kNotebookId, DocumentNotebook::onTabRightDown)
wxEND_EVENT_TABLE()
// clang-format on

DocumentNotebook::DocumentNotebook(wxWindow* parent, Context& ctx)
: wxAuiNotebook(parent, kNotebookId, wxDefaultPosition, wxDefaultSize, kNotebookStyle)
, m_ctx(ctx) {}

// ---------------------------------------------------------------------------
// Page management
// ---------------------------------------------------------------------------

void DocumentNotebook::addPage(Document& doc, const bool select) {
    AddPage(doc.getView(), doc.getTitle(), select);
}

void DocumentNotebook::removePage(Document& doc) {
    if (const auto idx = findIndex(doc); idx != wxNOT_FOUND) {
        DeletePage(static_cast<size_t>(idx));
    }
}

void DocumentNotebook::selectDocument(Document& doc) {
    if (const auto idx = findIndex(doc); idx != wxNOT_FOUND) {
        SetSelection(static_cast<size_t>(idx));
    }
}

void DocumentNotebook::updateTitle(const Document& doc) {
    if (const auto idx = findIndex(doc); idx != wxNOT_FOUND) {
        SetPageText(static_cast<size_t>(idx), doc.getTitle());
    }
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

auto DocumentNotebook::activeDocument() const -> Document* {
    const auto sel = GetSelection();
    if (sel == wxNOT_FOUND) {
        return nullptr;
    }
    return documentForPage(GetPage(static_cast<size_t>(sel)));
}

auto DocumentNotebook::documentForPage(const wxWindow* page) const -> Document* {
    if (page == nullptr) {
        return nullptr;
    }
    for (const auto& doc : m_ctx.getDocumentManager().getDocuments()) {
        if (doc->getView() == page) {
            return doc.get();
        }
    }
    return nullptr;
}

auto DocumentNotebook::findIndex(const Document& doc) const -> int {
    for (size_t idx = 0; idx < GetPageCount(); idx++) {
        if (GetPage(idx) == doc.getView()) {
            return static_cast<int>(idx);
        }
    }
    return wxNOT_FOUND;
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void DocumentNotebook::onPageClose(wxAuiNotebookEvent& event) {
    // Always veto — DocumentManager runs the modified-prompt + save +
    // intellisense-cancel pipeline and then drops the page itself.
    event.Veto();
    const auto pageIdx = event.GetSelection();
    if (pageIdx == wxNOT_FOUND) {
        return;
    }
    if (auto* doc = documentForPage(GetPage(static_cast<size_t>(pageIdx)))) {
        m_ctx.getDocumentManager().closeFile(*doc);
    }
}

void DocumentNotebook::onPageChanged(wxAuiNotebookEvent& event) {
    event.Skip();
    auto* doc = activeDocument();
    auto& ui = m_ctx.getUIManager();
    // Tab change shifts both the active document and (potentially)
    // the active project, so both command dimensions need a refresh.
    ui.syncDocCommands();
    ui.syncBuildCommands();
    if (doc == nullptr) {
        m_ctx.getSideBarManager().showSymbolsFor(nullptr);
        ui.setTitle(wxEmptyString);
        return;
    }
    doc->getEditor()->SetFocus();
    m_ctx.getSideBarManager().showSymbolsFor(doc);
    ui.setTitle(doc->getFrameTitle());
}

void DocumentNotebook::onBgDClick(wxAuiNotebookEvent& event) {
    event.Skip();
    m_ctx.getDocumentManager().newFile();
}

void DocumentNotebook::onTabRightDown(wxAuiNotebookEvent& event) {
    event.Skip();
    const auto pageIdx = event.GetSelection();
    if (pageIdx == wxNOT_FOUND) {
        return;
    }
    auto* doc = documentForPage(GetPage(static_cast<size_t>(pageIdx)));
    if (doc == nullptr) {
        return;
    }

    // Activate the right-clicked tab so commands dispatched via
    // CommandIds (Undo/Cut/etc.) act on it — they target the active
    // editor, which we just shifted.
    SetSelection(static_cast<size_t>(pageIdx));
    auto& dm = m_ctx.getDocumentManager();
    dm.syncEditCommands();

    auto& cmd = m_ctx.getCommandManager();
    const auto entryEnabled = [&cmd](const CommandId id) -> bool {
        const auto* entry = cmd.find(+id);
        return entry != nullptr && entry->isEnabled();
    };

    const bool hasOthers = dm.getCount() > 1;
    const bool hasPath = !doc->isNew();
    const auto path = doc->getFilePath();

    std::error_code fsEc;
    const bool fileOnDisk = hasPath && std::filesystem::exists(path, fsEc);

    wxMenu menu;
    menu.Append(+CommandId::Close, m_ctx.tr("commands.close.name"));
    menu.Append(kTabCloseOthersId, m_ctx.tr("tabContext.closeOthers"))
        ->Enable(hasOthers);
    menu.AppendSeparator();
    menu.Append(kTabShowInBrowserId, m_ctx.tr("tabContext.showInBrowser"))
        ->Enable(hasPath);
    menu.Append(kTabReloadFromDiskId, m_ctx.tr("tabContext.reloadFromDisk"))
        ->Enable(fileOnDisk);
    menu.AppendSeparator();
    menu.Append(+CommandId::Undo, m_ctx.tr("commands.undo.name"))
        ->Enable(entryEnabled(CommandId::Undo));
    menu.Append(+CommandId::Redo, m_ctx.tr("commands.redo.name"))
        ->Enable(entryEnabled(CommandId::Redo));
    menu.AppendSeparator();
    menu.Append(+CommandId::Cut, m_ctx.tr("commands.cut.name"))
        ->Enable(entryEnabled(CommandId::Cut));
    menu.Append(+CommandId::Copy, m_ctx.tr("commands.copy.name"))
        ->Enable(entryEnabled(CommandId::Copy));
    menu.Append(+CommandId::Paste, m_ctx.tr("commands.paste.name"))
        ->Enable(entryEnabled(CommandId::Paste));
    menu.Append(+CommandId::SelectAll, m_ctx.tr("commands.selectAll.name"))
        ->Enable(entryEnabled(CommandId::SelectAll));

    menu.Bind(wxEVT_MENU, [&dm, doc](const wxCommandEvent&) { dm.closeOtherFiles(*doc); }, kTabCloseOthersId);
    menu.Bind(
        wxEVT_MENU, [this, path](const wxCommandEvent&) {
            if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
                entry->setChecked(true);
            }
            m_ctx.getSideBarManager().locateFile(toWxString(path));
        },
        kTabShowInBrowserId
    );
    menu.Bind(
        wxEVT_MENU, [&dm, doc](const wxCommandEvent&) {
            if (dm.contains(doc)) {
                dm.reloadFromDisk(*doc);
            }
        },
        kTabReloadFromDiskId
    );

    PopupMenu(&menu);
}
