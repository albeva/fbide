//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolBrowser.hpp"
#include <wx/imaglist.h>
#include "analyses/symbols/SymbolTable.hpp"
#include "app/Context.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "ui/ArtiProvider.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {

/// Item data tag for symbol leaves. Folder items carry no data.
class SymbolItemData final : public wxTreeItemData {
public:
    explicit SymbolItemData(const int line)
    : m_line(line) {}

    [[nodiscard]] auto getLine() const -> int { return m_line; }

private:
    int m_line;
};

/// Append a folder + its symbols to the tree, but only if the bucket is
/// non-empty. Folder is rendered bold to separate categories from leaves.
/// Folder and leaf items share the per-kind icon from the image list
/// (index == static_cast<int>(kind)).
void appendBucket(wxTreeCtrl& tree, const wxTreeItemId& root,
    SymbolKind kind, const wxString& label, const std::vector<Symbol>& bucket) {
    if (bucket.empty()) {
        return;
    }
    const auto image = static_cast<int>(kind);
    const auto folder = tree.AppendItem(root, label, image, image);
    tree.SetItemBold(folder, true);
    for (const auto& sym : bucket) {
        tree.AppendItem(folder, sym.name, image, image, new SymbolItemData(sym.line));
    }
    tree.Expand(folder);
}

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(SymbolBrowser, wxTreeCtrl)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, SymbolBrowser::onItemActivated)
wxEND_EVENT_TABLE()
// clang-format on

SymbolBrowser::SymbolBrowser(Context& ctx, wxWindow* parent)
: wxTreeCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxTR_LINES_AT_ROOT)
, m_ctx(ctx) {
    wxTreeCtrl::AddRoot(wxEmptyString);

    // Image list — one bitmap per SymbolKind. Indices match the enum so
    // appendBucket can use static_cast<int>(kind) directly. AssignImageList
    // transfers ownership to the tree.
    constexpr std::array kIconKinds = {
        SymbolKind::Sub,
        SymbolKind::Function,
        SymbolKind::Type,
        SymbolKind::Union,
        SymbolKind::Enum,
    };
    const auto& art = m_ctx.getUIManager().getArtProvider();
    const auto sample = art.getBitmap(SymbolKind::Sub);
    const int iconW = sample.IsOk() ? sample.GetWidth() : 16;
    const int iconH = sample.IsOk() ? sample.GetHeight() : 16;
    const auto images = make_unowned<wxImageList>(iconW, iconH, true,
        static_cast<int>(kIconKinds.size()));
    for (const auto kind : kIconKinds) {
        images->Add(art.getBitmap(kind));
    }
    AssignImageList(images);
}

void SymbolBrowser::setSymbols(const Document* doc) {
    const auto table = (doc != nullptr) ? doc->getSymbolTable() : std::shared_ptr<const SymbolTable> {};
    if (table == m_currentTable) {
        return;
    }
    m_currentTable = table;
    if (table == nullptr) {
        clearTree();
    } else {
        rebuild(*table);
    }
}

void SymbolBrowser::clearTree() {
    DeleteAllItems();
    AddRoot(wxEmptyString);
}

void SymbolBrowser::rebuild(const SymbolTable& table) {
    Freeze();
    DeleteAllItems();
    const auto root = AddRoot(wxEmptyString);
    appendBucket(*this, root, SymbolKind::Sub,
        m_ctx.tr("sidebar.symbols.subs"), table.getSubs());
    appendBucket(*this, root, SymbolKind::Function,
        m_ctx.tr("sidebar.symbols.functions"), table.getFunctions());
    appendBucket(*this, root, SymbolKind::Type,
        m_ctx.tr("sidebar.symbols.types"), table.getTypes());
    appendBucket(*this, root, SymbolKind::Union,
        m_ctx.tr("sidebar.symbols.unions"), table.getUnions());
    appendBucket(*this, root, SymbolKind::Enum,
        m_ctx.tr("sidebar.symbols.enums"), table.getEnums());
    Thaw();
}

void SymbolBrowser::onItemActivated(wxTreeEvent& event) {
    const auto* data = dynamic_cast<SymbolItemData*>(GetItemData(event.GetItem()));
    if (data == nullptr) {
        return; // folder, not a symbol
    }
    const int line = data->getLine();
    // Defer focus + navigation via CallAfter: the tree reclaims focus once
    // its own ITEM_ACTIVATED processing finishes, so SetFocus mid-handler
    // gets stomped. Running after the event unwinds settles focus on the
    // editor as intended.
    CallAfter([this, line]() {
        auto* doc = m_ctx.getDocumentManager().getActive();
        if (doc == nullptr) {
            return;
        }
        auto* editor = doc->getEditor();
        if (editor == nullptr) {
            return;
        }
        editor->EnsureVisible(line);
        editor->GotoLine(line);
        editor->SetFirstVisibleLine(editor->VisibleFromDocLine(line));
        editor->SetFocus();
    });
}
