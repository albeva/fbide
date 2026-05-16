//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SymbolBrowser.hpp"
#include "analyses/symbols/SymbolTable.hpp"
#include "app/Context.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "ui/ArtiProvider.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

wxBEGIN_EVENT_TABLE(SymbolBrowser, wxTreeCtrl)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, SymbolBrowser::onItemActivated)
wxEND_EVENT_TABLE()

void SymbolBrowser::appendBucket(
    SymbolKind kind,
    const wxString& label,
    const std::vector<Symbol>& bucket
) {
    // Method-qualified symbols are nested under their owning type instead.
    const auto isFreeStanding = [](const Symbol& sym) { return symbolOwner(sym).empty(); };
    if (std::ranges::none_of(bucket, isFreeStanding)) {
        return;
    }

    const auto image = static_cast<int>(kind);
    const auto folder = AppendItem(GetRootItem(), label, image, image);
    SetItemBold(folder, true);
    for (std::size_t idx = 0; idx < bucket.size(); idx++) {
        const auto& sym = bucket[idx];
        if (!isFreeStanding(sym)) {
            continue;
        }
        const auto id = AppendItem(folder, sym.name, image, image, nullptr);
        m_entries[id.GetID()] = { .kind = kind, .index = idx };
    }
    Expand(folder);
}

auto SymbolBrowser::memberLabel(const Symbol& sym) const -> wxString {
    switch (sym.kind) {
    case SymbolKind::Constructor:
        return m_ctx.tr("sidebar.symbols.constructor");
    case SymbolKind::Destructor:
        return m_ctx.tr("sidebar.symbols.destructor");
    default:
        // `Owner.member` → bare member name.
        return sym.name.AfterLast('.');
    }
}

void SymbolBrowser::appendTypeTree(const wxString& label) {
    const auto& types = m_currentTable->getTypes();
    if (types.empty()) {
        return;
    }

    constexpr auto typeImage = static_cast<int>(SymbolKind::Type);
    const auto folder = AppendItem(GetRootItem(), label, typeImage, typeImage);
    SetItemBold(folder, true);

    // One nested member, paired with the flat-vector slot it navigates to.
    struct Member {
        SymbolKind kind;
        std::size_t index;
        int line;
        wxString label;
    };

    for (std::size_t typeIdx = 0; typeIdx < types.size(); typeIdx++) {
        const auto& type = types[typeIdx];
        const auto node = AppendItem(folder, type.name, typeImage, typeImage);
        // A declared type navigates to its source line; a synthetic owner
        // (negative line) is a group header only.
        if (type.line >= 0) {
            m_entries[node.GetID()] = { .kind = SymbolKind::Type, .index = typeIdx };
        }

        // Gather every member owned by this type, across all callable kinds.
        std::vector<Member> members;
        const auto gather = [&](SymbolKind kind, const std::vector<Symbol>& bucket) {
            for (std::size_t idx = 0; idx < bucket.size(); idx++) {
                if (symbolOwner(bucket[idx]) == type.name) {
                    members.push_back({ .kind = kind,
                        .index = idx,
                        .line = bucket[idx].line,
                        .label = memberLabel(bucket[idx]) });
                }
            }
        };
        gather(SymbolKind::Sub, m_currentTable->getSubs());
        gather(SymbolKind::Function, m_currentTable->getFunctions());
        gather(SymbolKind::Constructor, m_currentTable->getConstructors());
        gather(SymbolKind::Destructor, m_currentTable->getDestructors());
        gather(SymbolKind::Operator, m_currentTable->getOperators());
        gather(SymbolKind::Property, m_currentTable->getProperties());

        // Source order — members come from per-kind vectors, so sort by line.
        std::ranges::sort(members, {}, &Member::line);
        for (const auto& member : members) {
            const auto image = static_cast<int>(member.kind);
            const auto leaf = AppendItem(node, member.label, image, image, nullptr);
            m_entries[leaf.GetID()] = { .kind = member.kind, .index = member.index };
        }
        Expand(node);
    }
    Expand(folder);
}

void SymbolBrowser::appendIncludes(
    const wxString& label,
    const std::vector<Include>& includes
) {
    if (includes.empty()) {
        return;
    }

    constexpr auto image = static_cast<int>(SymbolKind::Include);
    const auto folder = AppendItem(GetRootItem(), label, image, image);
    SetItemBold(folder, true);
    for (std::size_t idx = 0; idx < includes.size(); idx++) {
        const auto& inc = includes[idx];
        const auto id = AppendItem(folder, inc.path, image, image, nullptr);
        m_entries[id.GetID()] = { .kind = SymbolKind::Include, .index = idx };
    }
    Expand(folder);
}

SymbolBrowser::SymbolBrowser(Context& ctx, wxWindow* parent)
: wxTreeCtrl(
      parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxTR_LINES_AT_ROOT
  )
, m_ctx(ctx) {
    wxTreeCtrl::AddRoot(wxEmptyString);

    // Image list — one bitmap per SymbolKind. Indices match the enum so
    // appendBucket can use static_cast<int>(kind) directly. AssignImageList
    // transfers ownership to the tree.
    constexpr std::array kIconKinds = {
        SymbolKind::Sub,
        SymbolKind::Function,
        SymbolKind::Constructor,
        SymbolKind::Destructor,
        SymbolKind::Operator,
        SymbolKind::Property,
        SymbolKind::Type,
        SymbolKind::Union,
        SymbolKind::Enum,
        SymbolKind::Macro,
        SymbolKind::Include,
    };
    const auto& art = m_ctx.getUIManager().getArtProvider();
    const auto sample = art.getBitmap(SymbolKind::Sub);
    const int iconW = sample.IsOk() ? sample.GetWidth() : 16;
    const int iconH = sample.IsOk() ? sample.GetHeight() : 16;
    const auto images = make_unowned<wxImageList>(iconW, iconH, true, static_cast<int>(kIconKinds.size()));
    for (const auto kind : kIconKinds) {
        images->Add(art.getBitmap(kind));
    }
    AssignImageList(images);
}

void SymbolBrowser::setSymbols(const Document* doc) {
    const auto table = doc != nullptr ? doc->getSymbolTable() : nullptr;
    if (table == m_currentTable) {
        return;
    }
    const auto* old = m_currentTable.get();
    m_currentTable = table;

    if (table == nullptr) {
        clearTree();
    } else if (old == nullptr || old->getHash() != table->getHash()) {
        rebuild();
    }
}

void SymbolBrowser::clearTree() {
    m_entries.clear();
    DeleteAllItems();
    AddRoot(wxEmptyString);
}

void SymbolBrowser::rebuild() {
    const auto thaw = FreezeLock(this);

    clearTree();
    appendIncludes(m_ctx.tr("sidebar.symbols.includes"), m_currentTable->getIncludes());
    appendTypeTree(m_ctx.tr("sidebar.symbols.types"));
    appendBucket(SymbolKind::Sub, m_ctx.tr("sidebar.symbols.subs"), m_currentTable->getSubs());
    appendBucket(SymbolKind::Function, m_ctx.tr("sidebar.symbols.functions"), m_currentTable->getFunctions());
    appendBucket(SymbolKind::Operator, m_ctx.tr("sidebar.symbols.operators"), m_currentTable->getOperators());
    appendBucket(SymbolKind::Union, m_ctx.tr("sidebar.symbols.unions"), m_currentTable->getUnions());
    appendBucket(SymbolKind::Enum, m_ctx.tr("sidebar.symbols.enums"), m_currentTable->getEnums());
    appendBucket(SymbolKind::Macro, m_ctx.tr("sidebar.symbols.macros"), m_currentTable->getMacros());
}

void SymbolBrowser::onItemActivated(wxTreeEvent& event) {
    const auto item = event.GetItem();
    if (!item.IsOk() || m_currentTable == nullptr) {
        return;
    }

    const auto iter = m_entries.find(item.GetID());
    if (iter == m_entries.end()) {
        return;
    }

    dispatch(iter->second);
}

void SymbolBrowser::dispatch(const Entry& entry) {
    const auto& table = *m_currentTable;

    const auto gotoSymbol = [this](const std::vector<Symbol>& vec, const std::size_t idx) {
        const int line = vec.at(idx).line;
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
    };

    switch (entry.kind) {
    case SymbolKind::Include: {
        const auto& vec = table.getIncludes();
        const wxString path = vec[entry.index].path;
        CallAfter([this, path]() {
            auto& docMgr = m_ctx.getDocumentManager();
            const auto* origin = docMgr.getActive();
            if (origin == nullptr) {
                return;
            }
            docMgr.openInclude(*origin, path);
        });
        return;
    }
    case SymbolKind::Sub:
        gotoSymbol(table.getSubs(), entry.index);
        break;
    case SymbolKind::Function:
        gotoSymbol(table.getFunctions(), entry.index);
        break;
    case SymbolKind::Constructor:
        gotoSymbol(table.getConstructors(), entry.index);
        break;
    case SymbolKind::Destructor:
        gotoSymbol(table.getDestructors(), entry.index);
        break;
    case SymbolKind::Operator:
        gotoSymbol(table.getOperators(), entry.index);
        break;
    case SymbolKind::Property:
        gotoSymbol(table.getProperties(), entry.index);
        break;
    case SymbolKind::Type:
        gotoSymbol(table.getTypes(), entry.index);
        break;
    case SymbolKind::Union:
        gotoSymbol(table.getUnions(), entry.index);
        break;
    case SymbolKind::Enum:
        gotoSymbol(table.getEnums(), entry.index);
        break;
    case SymbolKind::Macro:
        gotoSymbol(table.getMacros(), entry.index);
        break;
    }
}
