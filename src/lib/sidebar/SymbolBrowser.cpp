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

auto fbide::parseSymbolFilter(const wxString& query) -> std::vector<wxString> {
    std::vector<wxString> words;
    wxStringTokenizer tokenizer(query, " \t\r\n", wxTOKEN_STRTOK);
    while (tokenizer.HasMoreTokens()) {
        words.push_back(tokenizer.GetNextToken().Lower());
    }
    return words;
}

auto fbide::symbolFilterMatches(const std::vector<wxString>& words, const wxString& haystack) -> bool {
    return std::ranges::all_of(words, [&](const wxString& word) { return haystack.Contains(word); });
}

wxBEGIN_EVENT_TABLE(SymbolBrowser, wxTreeCtrl)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, SymbolBrowser::onItemActivated)
wxEND_EVENT_TABLE()

auto SymbolBrowser::kindKeywords(const SymbolKind kind) -> wxString {
    switch (kind) {
    case SymbolKind::Sub:
        return "sub";
    case SymbolKind::Function:
        return "function";
    case SymbolKind::Constructor:
        return "constructor";
    case SymbolKind::Destructor:
        return "destructor";
    case SymbolKind::Operator:
        return "operator";
    case SymbolKind::Property:
        return "property properties";
    case SymbolKind::Type:
        return "type udt";
    case SymbolKind::Union:
        return "union";
    case SymbolKind::Enum:
        return "enum";
    case SymbolKind::Macro:
        return "macro";
    case SymbolKind::Include:
        return "include";
    }
    return {};
}

auto SymbolBrowser::kindLocaleLabel(const SymbolKind kind) const -> const wxString& {
    // Memoized at construction — `passesFilter` calls this per symbol per
    // keystroke, and a live `tr()` lookup each time is wasteful. The locale
    // never changes mid-session (a language change restarts the IDE).
    return m_kindLabels[static_cast<std::size_t>(kind)];
}

auto SymbolBrowser::localeLabelFor(const SymbolKind kind) const -> wxString {
    switch (kind) {
    case SymbolKind::Sub:
        return m_ctx.tr("sidebar.symbols.subs");
    case SymbolKind::Function:
        return m_ctx.tr("sidebar.symbols.functions");
    case SymbolKind::Constructor:
        return m_ctx.tr("sidebar.symbols.constructors");
    case SymbolKind::Destructor:
        return m_ctx.tr("sidebar.symbols.destructors");
    case SymbolKind::Operator:
        return m_ctx.tr("sidebar.symbols.operators");
    case SymbolKind::Property:
        return m_ctx.tr("sidebar.symbols.properties");
    case SymbolKind::Type:
        return m_ctx.tr("sidebar.symbols.types");
    case SymbolKind::Union:
        return m_ctx.tr("sidebar.symbols.unions");
    case SymbolKind::Enum:
        return m_ctx.tr("sidebar.symbols.enums");
    case SymbolKind::Macro:
        return m_ctx.tr("sidebar.symbols.macros");
    case SymbolKind::Include:
        return m_ctx.tr("sidebar.symbols.includes");
    }
    return {};
}

auto SymbolBrowser::filterHaystack(const wxString& name, SymbolKind kind) const -> wxString {
    // Owner-qualified names (`Owner.member`) carry the owning UDT, so a
    // member also matches when its type name is searched for.
    wxString haystack = name;
    haystack << ' ' << kindKeywords(kind) << ' ' << kindLocaleLabel(kind);
    return haystack.Lower();
}

auto SymbolBrowser::passesFilter(const wxString& name, SymbolKind kind) const -> bool {
    if (m_filterWords.empty()) {
        return true;
    }
    return symbolFilterMatches(m_filterWords, filterHaystack(name, kind));
}

auto SymbolBrowser::sourceTables() const -> std::vector<const SymbolTable*> {
    std::vector<const SymbolTable*> tables;
    if (m_currentTable != nullptr) {
        tables.push_back(m_currentTable.get());
        for (const auto& imported : m_currentTable->getImported()) {
            if (imported != nullptr) {
                tables.push_back(imported.get());
            }
        }
    }
    return tables;
}

void SymbolBrowser::appendBucket(
    SymbolKind kind,
    const wxString& label,
    BucketGetter getter
) {
    // Only the current document's symbols appear at top level; imported symbols
    // are nested under their include in the Includes group. Method-qualified
    // symbols are nested under their owning type instead.
    const auto isFreeStanding = [](const Symbol& sym) { return symbolOwner(sym).empty(); };
    const auto& bucket = (m_currentTable.get()->*getter)();

    std::vector<std::size_t> kept;
    for (std::size_t idx = 0; idx < bucket.size(); idx++) {
        if (isFreeStanding(bucket[idx]) && passesFilter(bucket[idx].name, kind)) {
            kept.push_back(idx);
        }
    }
    if (kept.empty()) {
        return;
    }

    const auto image = static_cast<int>(kind);
    const auto folder = AppendItem(GetRootItem(), label, image, image);
    SetItemBold(folder, true);
    for (const auto idx : kept) {
        const auto id = AppendItem(folder, bucket[idx].name, image, image, nullptr);
        m_entries[id.GetID()] = { .tableIndex = 0, .kind = kind, .index = idx };
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

    // One nested member, paired with the flat-vector slot it navigates to.
    struct Member {
        SymbolKind kind;
        std::size_t index;
        int line;
        wxString label;
    };
    // A type that survived the filter, paired with its surviving members.
    struct TypeNode {
        std::size_t typeIdx;
        std::vector<Member> members;
    };

    std::vector<TypeNode> kept;
    for (std::size_t typeIdx = 0; typeIdx < types.size(); typeIdx++) {
        const auto& type = types[typeIdx];
        // A matching UDT pulls in all of its members; otherwise each member must
        // match the filter on its own.
        const bool typePasses = passesFilter(type.name, SymbolKind::Type);

        std::vector<Member> members;
        const auto gather = [&](SymbolKind kind, const std::vector<Symbol>& bucket) {
            for (std::size_t idx = 0; idx < bucket.size(); idx++) {
                const auto& sym = bucket[idx];
                if (symbolOwner(sym) != type.name) {
                    continue;
                }
                if (!typePasses && !passesFilter(sym.name, kind)) {
                    continue;
                }
                members.push_back({ .kind = kind,
                    .index = idx,
                    .line = sym.line,
                    .label = memberLabel(sym) });
            }
        };
        gather(SymbolKind::Sub, m_currentTable->getSubs());
        gather(SymbolKind::Function, m_currentTable->getFunctions());
        gather(SymbolKind::Constructor, m_currentTable->getConstructors());
        gather(SymbolKind::Destructor, m_currentTable->getDestructors());
        gather(SymbolKind::Operator, m_currentTable->getOperators());
        gather(SymbolKind::Property, m_currentTable->getProperties());

        if (!typePasses && members.empty()) {
            continue;
        }
        // Source order — members come from per-kind vectors, so sort by line.
        std::ranges::sort(members, {}, &Member::line);
        kept.push_back({ .typeIdx = typeIdx, .members = std::move(members) });
    }
    if (kept.empty()) {
        return;
    }

    const auto folder = AppendItem(GetRootItem(), label, typeImage, typeImage);
    SetItemBold(folder, true);
    for (const auto& typeNode : kept) {
        const auto& type = types[typeNode.typeIdx];
        const auto node = AppendItem(folder, type.name, typeImage, typeImage);
        // A declared type navigates to its source line; a synthetic owner
        // (negative line) is a group header only.
        if (type.line >= 0) {
            m_entries[node.GetID()] = { .tableIndex = 0, .kind = SymbolKind::Type, .index = typeNode.typeIdx };
        }
        for (const auto& member : typeNode.members) {
            const auto image = static_cast<int>(member.kind);
            const auto leaf = AppendItem(node, member.label, image, image, nullptr);
            m_entries[leaf.GetID()] = { .tableIndex = 0, .kind = member.kind, .index = member.index };
        }
        Expand(node);
    }
    Expand(folder);
}

void SymbolBrowser::appendIncludes(
    const wxString& label,
    const std::vector<Include>& includes
) {
    if (m_currentTable == nullptr) {
        return;
    }
    // Same index space dispatch() resolves against, so a leaf's tableIndex maps
    // back to the right table (sourceTables() filters nulls; [0] is the document).
    const auto tables = sourceTables();

    // The API an included file contributes: typenames, free-standing callables
    // and macros (methods stay with their type; enum members are completion
    // only). `all` keeps everything (used when the include path itself matched
    // the filter), otherwise each symbol must pass the filter on its own.
    struct ApiSym {
        SymbolKind kind;
        std::size_t index;
        wxString name;
    };
    const auto collectApi = [this](const SymbolTable& table, const bool all) {
        std::vector<ApiSym> out;
        const auto add = [&](SymbolKind kind, const std::vector<Symbol>& vec, const bool freeOnly) {
            for (std::size_t idx = 0; idx < vec.size(); idx++) {
                if (freeOnly && !symbolOwner(vec[idx]).empty()) {
                    continue;
                }
                if (!all && !passesFilter(vec[idx].name, kind)) {
                    continue;
                }
                out.push_back({ .kind = kind, .index = idx, .name = vec[idx].name });
            }
        };
        add(SymbolKind::Type, table.getTypes(), false);
        add(SymbolKind::Union, table.getUnions(), false);
        add(SymbolKind::Enum, table.getEnums(), false);
        add(SymbolKind::Sub, table.getSubs(), true);
        add(SymbolKind::Function, table.getFunctions(), true);
        add(SymbolKind::Operator, table.getOperators(), true);
        add(SymbolKind::Property, table.getProperties(), true);
        add(SymbolKind::Macro, table.getMacros(), false);
        return out;
    };

    constexpr auto image = static_cast<int>(SymbolKind::Include);
    wxTreeItemId folder; // created lazily, once an include survives the filter

    // Resolved imported tables keyed by file name (first match wins, mirroring
    // the original ascending scan) so each directive matches in O(1) rather than
    // re-scanning every table per include.
    std::unordered_map<std::filesystem::path, std::pair<std::size_t, const SymbolTable*>> tableByName;
    for (std::size_t ti = 1; ti < tables.size(); ti++) { // skip [0] = the document
        tableByName.try_emplace(tables[ti]->getSourcePath().filename(), ti, tables[ti]);
    }

    for (std::size_t idx = 0; idx < includes.size(); idx++) {
        const bool pathMatches = passesFilter(includes[idx].path, SymbolKind::Include);

        // Match this directive to its resolved imported table by file name.
        const auto wantedName = std::filesystem::path(includes[idx].path.utf8_string()).filename();
        const SymbolTable* importedTable = nullptr;
        std::size_t tableIndex = 0;
        if (const auto it = tableByName.find(wantedName); it != tableByName.end()) {
            tableIndex = it->second.first;
            importedTable = it->second.second;
        }

        std::vector<ApiSym> api;
        if (importedTable != nullptr) {
            api = collectApi(*importedTable, pathMatches);
        }
        if (!pathMatches && api.empty()) {
            continue; // neither the include nor any of its symbols match the filter
        }

        if (!folder.IsOk()) {
            folder = AppendItem(GetRootItem(), label, image, image);
            SetItemBold(folder, true);
        }
        const auto node = AppendItem(folder, includes[idx].path, image, image);
        m_entries[node.GetID()] = { .tableIndex = 0, .kind = SymbolKind::Include, .index = idx };
        for (const auto& sym : api) {
            const auto symImage = static_cast<int>(sym.kind);
            const auto leaf = AppendItem(node, sym.name, symImage, symImage, nullptr);
            m_entries[leaf.GetID()] = { .tableIndex = tableIndex, .kind = sym.kind, .index = sym.index };
        }
        // The include node is left collapsed so imports stay out of the way.
    }
    if (folder.IsOk()) {
        Expand(folder);
    }
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

    // Memoize the localized kind labels once — passesFilter reads them per
    // symbol per keystroke (see kindLocaleLabel).
    for (std::size_t i = 0; i < m_kindLabels.size(); i++) {
        m_kindLabels[i] = localeLabelFor(static_cast<SymbolKind>(i));
    }
}

void SymbolBrowser::setSymbols(const Document* doc) {
    const auto table = doc != nullptr ? doc->getSymbolTable() : nullptr;
    if (table == m_currentTable) {
        return;
    }
    // The tree now renders the document plus its #include closure, so rebuild
    // when either changes — hash the own symbols combined with each import's.
    const auto closureHash = [](const SymbolTable* tbl) -> std::size_t {
        if (tbl == nullptr) {
            return 0;
        }
        constexpr std::size_t kMix = sizeof(std::size_t) >= 8 ? 0x9e3779b97f4a7c15ULL : 0x9e3779b9UL;
        std::size_t hash = tbl->getHash();
        for (const auto& imported : tbl->getImported()) {
            if (imported != nullptr) {
                hash ^= imported->getHash() + kMix + (hash << 6) + (hash >> 2);
            }
        }
        return hash;
    };
    const auto* old = m_currentTable.get();
    const bool changed = old == nullptr || closureHash(old) != closureHash(table.get());
    m_currentTable = table;

    if (table == nullptr) {
        clearTree();
    } else if (changed) {
        rebuild();
    }
}

void SymbolBrowser::setFilter(const wxString& query) {
    auto words = parseSymbolFilter(query);
    if (words == m_filterWords) {
        return;
    }
    m_filterWords = std::move(words);
    if (m_currentTable != nullptr) {
        rebuild();
    }
}

void SymbolBrowser::clearTree() {
    m_entries.clear();
    DeleteAllItems();
    AddRoot(wxEmptyString);
}

void SymbolBrowser::rebuild() {
    // Repopulating a native wxTreeCtrl (DeleteAllItems + AppendItem + Expand)
    // pulls keyboard focus onto the control on MSW. A rebuild runs on a
    // background intellisense result while the user is typing in the editor,
    // so the browser must never steal focus — capture whatever window holds
    // it and restore it if the rebuild moved it.
    wxWindow* const priorFocus = wxWindow::FindFocus();

    {
        const auto thaw = FreezeLock(this);

        clearTree();
        appendIncludes(m_ctx.tr("sidebar.symbols.includes"), m_currentTable->getIncludes());
        appendTypeTree(m_ctx.tr("sidebar.symbols.types"));
        appendBucket(SymbolKind::Sub, m_ctx.tr("sidebar.symbols.subs"), &SymbolTable::getSubs);
        appendBucket(SymbolKind::Function, m_ctx.tr("sidebar.symbols.functions"), &SymbolTable::getFunctions);
        appendBucket(SymbolKind::Operator, m_ctx.tr("sidebar.symbols.operators"), &SymbolTable::getOperators);
        appendBucket(SymbolKind::Union, m_ctx.tr("sidebar.symbols.unions"), &SymbolTable::getUnions);
        appendBucket(SymbolKind::Enum, m_ctx.tr("sidebar.symbols.enums"), &SymbolTable::getEnums);
        appendBucket(SymbolKind::Macro, m_ctx.tr("sidebar.symbols.macros"), &SymbolTable::getMacros);
    }

    if (priorFocus != nullptr && wxWindow::FindFocus() != priorFocus) {
        priorFocus->SetFocus();
    }
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
    // Resolve the table live (never store a raw pointer in the entry — the table
    // can be replaced without a rebuild on a hash-equal swap). Bounds-checked
    // against the rare case the structure shifted under a skipped rebuild.
    const auto tables = sourceTables();
    if (entry.tableIndex >= tables.size()) {
        return;
    }
    const SymbolTable* const table = tables[entry.tableIndex];
    const bool isCurrent = entry.tableIndex == 0;

    // Navigate to a symbol's line. Own-document symbols use the active editor;
    // imported ones open their source file first.
    const auto gotoSymbol = [this, table, isCurrent](const std::vector<Symbol>& vec, const std::size_t idx) {
        if (idx >= vec.size()) {
            return;
        }
        const int line = vec[idx].line;
        const auto path = table->getSourcePath();
        CallAfter([this, line, isCurrent, path]() {
            auto& docMgr = m_ctx.getDocumentManager();
            auto* doc = (isCurrent || path.empty()) ? docMgr.getActive() : docMgr.openFile(path);
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
        if (entry.index >= table->getIncludes().size()) {
            return;
        }
        const wxString path = table->getIncludes()[entry.index].path;
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
        gotoSymbol(table->getSubs(), entry.index);
        break;
    case SymbolKind::Function:
        gotoSymbol(table->getFunctions(), entry.index);
        break;
    case SymbolKind::Constructor:
        gotoSymbol(table->getConstructors(), entry.index);
        break;
    case SymbolKind::Destructor:
        gotoSymbol(table->getDestructors(), entry.index);
        break;
    case SymbolKind::Operator:
        gotoSymbol(table->getOperators(), entry.index);
        break;
    case SymbolKind::Property:
        gotoSymbol(table->getProperties(), entry.index);
        break;
    case SymbolKind::Type:
        gotoSymbol(table->getTypes(), entry.index);
        break;
    case SymbolKind::Union:
        gotoSymbol(table->getUnions(), entry.index);
        break;
    case SymbolKind::Enum:
        gotoSymbol(table->getEnums(), entry.index);
        break;
    case SymbolKind::Macro:
        gotoSymbol(table->getMacros(), entry.index);
        break;
    }
}
