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

/// Split a raw search query into lowercased, non-empty filter words.
/// Whitespace-delimited; an all-blank query yields an empty list.
[[nodiscard]] auto parseSymbolFilter(const wxString& query) -> std::vector<wxString>;

/// True when every word in `words` is a substring of `haystack`. Both
/// `words` and `haystack` must already be lowercased. An empty `words`
/// list matches anything.
[[nodiscard]] auto symbolFilterMatches(const std::vector<wxString>& words, const wxString& haystack) -> bool;

/**
 * Sub/Function tree tab — a `wxTreeCtrl` subclass parented to the
 * sidebar notebook (inside `SymbolBrowserPanel`). Renders a
 * `Document`'s `SymbolTable`; activating a leaf jumps the active
 * editor to that line and scrolls it to the top of the viewport.
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

    /// Construct, parented to the sidebar notebook.
    SymbolBrowser(Context& ctx, wxWindow* parent);
    /// Trivial destructor.
    ~SymbolBrowser() override = default;

    /// Repopulate from `doc`'s current SymbolTable. `nullptr` (or a doc
    /// with no parsed table yet) clears it. Skips the tree rebuild when
    /// the new table is the same `shared_ptr` as the current one OR its
    /// hash matches — line-only edits don't visually change the listing.
    void setSymbols(const Document* doc);

    /// Apply a live filter query. Parses `query` into words and rebuilds
    /// the tree showing only matching entries. An empty/blank query shows
    /// everything. No-op when the parsed words are unchanged.
    void setFilter(const wxString& query);

private:
    /// Per-leaf lookup payload. `kind` selects which `SymbolTable` vector
    /// to read, `index` is the offset into that vector.
    struct Entry {
        std::size_t tableIndex; ///< Index into `sourceTables()` (0 = document, 1.. = imports).
        SymbolKind kind;        ///< Which `SymbolTable` bucket the leaf came from.
        std::size_t index;      ///< Offset into that bucket's vector.
    };

    /// Pointer-to-member accessor for a symbol bucket (e.g. `&SymbolTable::getSubs`).
    using BucketGetter = const std::vector<Symbol>& (SymbolTable::*)() const;

    /// Tree-leaf activation — dispatch to navigation or include open.
    void onItemActivated(wxTreeEvent& event);
    /// Rebuild the tree from `m_currentTable`.
    void rebuild();
    /// Clear the tree and `m_entries`.
    void clearTree();

    /// Append a folder + leaves under the tree root for every free-standing
    /// symbol in `bucket` that passes the current filter. Method-qualified
    /// symbols are skipped — they are grouped under their owning type by
    /// `appendTypeTree`. The folder is omitted when nothing survives. Each
    /// leaf registers an `Entry` in `m_entries` keyed by its tree id.
    void appendBucket(SymbolKind kind, const wxString& label, BucketGetter getter);

    /// The tables whose symbols are rendered: the current document followed by
    /// its `#include` closure (so imported symbols appear in the same buckets).
    [[nodiscard]] auto sourceTables() const -> std::vector<const SymbolTable*>;

    /// English keyword synonyms for a kind (e.g. `Type` → "type udt"),
    /// space-joined and lowercased — part of a leaf's filter haystack.
    [[nodiscard]] static auto kindKeywords(SymbolKind kind) -> wxString;

    /// Localised group label for a kind (the `sidebar.symbols.*` string) —
    /// the locale-specific half of a leaf's filter haystack. Memoized; see
    /// `m_kindLabels`.
    [[nodiscard]] auto kindLocaleLabel(SymbolKind kind) const -> const wxString&;

    /// Resolve the localised label for a kind via `tr()` (used once to fill
    /// `m_kindLabels`).
    [[nodiscard]] auto localeLabelFor(SymbolKind kind) const -> wxString;

    /// Build the lowercased filter haystack for one entry: its (possibly
    /// owner-qualified) `name` plus the kind's english + localised words.
    [[nodiscard]] auto filterHaystack(const wxString& name, SymbolKind kind) const -> wxString;

    /// True when `name`/`kind` passes the current filter (always true when
    /// no filter is set).
    [[nodiscard]] auto passesFilter(const wxString& name, SymbolKind kind) const -> bool;

    /// Append the Types folder: each declared or synthesised type, with its
    /// method members (subs, functions, constructors, …) nested beneath it.
    /// Declared types register a navigable `Entry`; synthetic group-only
    /// types do not.
    void appendTypeTree(const wxString& label);

    /// Display label for a member leaf: the bare member name (`Owner.member`
    /// stripped to `member`), or a localised "Constructor" / "Destructor".
    [[nodiscard]] auto memberLabel(const Symbol& sym) const -> wxString;

    /// Append the Includes folder under the tree root when non-empty.
    /// Each leaf registers an `Entry` in `m_entries`.
    void appendIncludes(const wxString& label, const std::vector<Include>& includes);

    /// Resolve a leaf entry to its action: navigate the active editor to
    /// the symbol's line, or open the included file.
    void dispatch(const Entry& entry);

    Context& m_ctx;                                    ///< Application context.
    std::shared_ptr<const SymbolTable> m_currentTable; ///< Currently rendered symbol table.
    std::vector<wxString> m_filterWords;               ///< Active filter words (lowercased); empty = no filter.
    /// Memoized localized kind labels, indexed by `SymbolKind`. Filled once
    /// in the constructor (the locale is fixed for a session).
    std::array<wxString, static_cast<std::size_t>(SymbolKind::Include) + 1> m_kindLabels;

    /// Tree id → entry payload. Rebuilt in `rebuild`, cleared in `clearTree`.
    /// `wxTreeItemId::Type` is the underlying void* the control hands out.
    std::unordered_map<wxTreeItemId::Type, Entry> m_entries;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
