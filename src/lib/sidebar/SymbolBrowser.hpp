//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/symbols/SymbolTable.hpp"

namespace fbide {
class Context;
class Document;

/**
 * Sub/Function tree tab — a `wxTreeCtrl` subclass parented to the
 * sidebar notebook. Renders a `Document`'s `SymbolTable`; activating
 * a leaf jumps the active editor to that line and scrolls it to the
 * top of the viewport.
 *
 * Subclassing the control directly (rather than `PushEventHandler`)
 * means the static event table dispatches without an extra handler
 * pushed onto the wx stack — no teardown-order races during frame
 * close.
 *
 * See @ref analyses.
 */
class SymbolBrowser final : public wxTreeCtrl {
public:
    NO_COPY_AND_MOVE(SymbolBrowser)

    SymbolBrowser(Context& ctx, wxWindow* parent);
    ~SymbolBrowser() override = default;

    /// Repopulate from `doc`'s current SymbolTable. `nullptr` (or a doc
    /// with no parsed table yet) clears it. Skips the tree rebuild when
    /// the new table is the same `shared_ptr` as the current one OR its
    /// hash matches — line-only edits don't visually change the listing.
    void setSymbols(const Document* doc);

private:
    /// Per-leaf lookup payload. `kind` selects which `SymbolTable` vector
    /// to read, `index` is the offset into that vector.
    struct Entry {
        SymbolKind  kind;
        std::size_t index;
    };

    void onItemActivated(wxTreeEvent& event);
    void rebuild();
    void clearTree();

    /// Append a folder + leaves under the tree root when the bucket is
    /// non-empty. Each leaf registers an `Entry` in `m_entries` keyed by
    /// its tree id, so activation skips any tree-walking.
    void appendBucket(SymbolKind kind, const wxString& label, const std::vector<Symbol>& bucket);

    /// Append the Includes folder under the tree root when non-empty.
    /// Each leaf registers an `Entry` in `m_entries`.
    void appendIncludes(const wxString& label, const std::vector<Include>& includes);

    /// Resolve a leaf entry to its action: navigate the active editor to
    /// the symbol's line, or open the included file.
    void dispatch(const Entry& entry);

    Context& m_ctx;
    std::shared_ptr<const SymbolTable> m_currentTable;

    /// Tree id → entry payload. Rebuilt in `rebuild`, cleared in `clearTree`.
    /// `wxTreeItemId::Type` is the underlying void* the control hands out.
    std::unordered_map<wxTreeItemId::Type, Entry> m_entries;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
