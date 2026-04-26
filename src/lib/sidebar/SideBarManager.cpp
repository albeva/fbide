//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include <wx/dirctrl.h>
#include <wx/treectrl.h>
#include "analyses/symbols/SymbolTable.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
using namespace fbide;

namespace {
const int BrowserTabsId = wxNewId();

/// Item data tag for symbol leaves. Folder items carry no data.
class SymbolItemData final : public wxTreeItemData {
public:
    explicit SymbolItemData(int line)
    : m_line(line) {}
    [[nodiscard]] auto getLine() const -> int { return m_line; }

private:
    int m_line;
};

/// Append a folder + its symbols to the tree, but only if the bucket is
/// non-empty. Matches the old fbide behaviour of showing only categories
/// that have entries.
void appendBucket(wxTreeCtrl& tree, const wxTreeItemId& root,
    const wxString& label, const std::vector<Symbol>& bucket) {
    if (bucket.empty()) {
        return;
    }
    const auto folder = tree.AppendItem(root, label);
    for (const auto& sym : bucket) {
        tree.AppendItem(folder, sym.name, -1, -1, new SymbolItemData(sym.line));
    }
    tree.Expand(folder);
}
} // namespace

SideBarManager::SideBarManager(Context& ctx)
: m_ctx(ctx) {}

SideBarManager::~SideBarManager() = default;

void SideBarManager::attach(wxAuiNotebook* notebook) {
    if (notebook == nullptr) {
        wxLogError("SideBarManager::attach called with null notebook");
        return;
    }
    m_notebook = notebook;

    // Sub/Function tree tab — first page so it matches the old fbide order
    // (S/F tree → Browse Files).
    m_symbolTree = make_unowned<wxTreeCtrl>(
        m_notebook,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxTR_LINES_AT_ROOT
    );
    m_symbolTree->AddRoot(wxEmptyString);
    m_symbolTree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &SideBarManager::onSymbolItemActivated, this);

    m_subFunctionPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_symbolTree, m_ctx.tr("sidebar.tabs.subFunction"));

    // Browse Files tab.
    m_dirCtrl = make_unowned<wxGenericDirCtrl>(
        m_notebook,
        BrowserTabsId,
        wxDirDialogDefaultFolderStr,
        wxDefaultPosition,
        wxDefaultSize,
        wxDIRCTRL_3D_INTERNAL,
        wxEmptyString
    );
    m_dirCtrl->Bind(wxEVT_DIRCTRL_FILEACTIVATED, &SideBarManager::onFileActivated, this);

    m_browseFilesPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_dirCtrl, m_ctx.tr("sidebar.tabs.browseFiles"));
}

void SideBarManager::locateFile(const wxString& path) {
    if (path.IsEmpty() || m_dirCtrl == nullptr || m_notebook == nullptr) {
        return;
    }
    if (m_browseFilesPage != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(m_browseFilesPage));
    }
    m_dirCtrl->ExpandPath(path);
    m_dirCtrl->SelectPath(path);
}

void SideBarManager::showSymbolBrowser() {
    if (m_notebook == nullptr || m_subFunctionPage == wxNOT_FOUND) {
        return;
    }
    // Show the pane: setChecked drives the AUI pane via the CommandEntry's
    // wxAuiManager bind (same path the View → Browser menu uses).
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
        entry->setChecked(true);
    }
    m_notebook->SetSelection(static_cast<size_t>(m_subFunctionPage));
    if (m_symbolTree != nullptr) {
        m_symbolTree->SetFocus();
    }
}

void SideBarManager::showSymbolsFor(const Document* doc) {
    if (m_symbolTree == nullptr) {
        return;
    }
    auto table = (doc != nullptr) ? doc->getSymbolTable() : std::shared_ptr<const SymbolTable> {};
    if (table == m_currentTable) {
        return; // same instance — no work
    }
    m_currentTable = table;
    if (table == nullptr) {
        clearSymbolTree();
    } else {
        rebuildSymbolTree(*table);
    }
}

void SideBarManager::clearSymbolTree() {
    m_symbolTree->DeleteAllItems();
    m_symbolTree->AddRoot(wxEmptyString);
}

void SideBarManager::rebuildSymbolTree(const SymbolTable& table) {
    m_symbolTree->Freeze();
    m_symbolTree->DeleteAllItems();
    const auto root = m_symbolTree->AddRoot(wxEmptyString);
    appendBucket(*m_symbolTree, root, m_ctx.tr("sidebar.symbols.subs"), table.getSubs());
    appendBucket(*m_symbolTree, root, m_ctx.tr("sidebar.symbols.functions"), table.getFunctions());
    appendBucket(*m_symbolTree, root, m_ctx.tr("sidebar.symbols.types"), table.getTypes());
    appendBucket(*m_symbolTree, root, m_ctx.tr("sidebar.symbols.unions"), table.getUnions());
    appendBucket(*m_symbolTree, root, m_ctx.tr("sidebar.symbols.enums"), table.getEnums());
    m_symbolTree->Thaw();
}

void SideBarManager::onFileActivated(wxTreeEvent& event) {
    event.Skip();
    if (m_dirCtrl == nullptr) {
        return;
    }
    const auto path = m_dirCtrl->GetFilePath();
    if (path.IsEmpty()) {
        return;
    }
    m_ctx.getDocumentManager().openFile(path);
}

void SideBarManager::onSymbolItemActivated(wxTreeEvent& event) {
    event.Skip();
    if (m_symbolTree == nullptr) {
        return;
    }
    const auto* data = dynamic_cast<SymbolItemData*>(m_symbolTree->GetItemData(event.GetItem()));
    if (data == nullptr) {
        return; // folder, not a symbol
    }
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    auto* editor = doc->getEditor();
    if (editor == nullptr) {
        return;
    }
    editor->GotoLine(data->getLine());
    editor->SetFocus();
}
