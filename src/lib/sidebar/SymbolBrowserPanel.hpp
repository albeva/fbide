//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

class wxSearchCtrl;
class wxCommandEvent;

namespace fbide {
class Context;
class Document;
class SymbolBrowser;

/**
 * Sub/Function sidebar tab: a live filter box stacked above the
 * `SymbolBrowser` tree. Typing in the box filters the tree in real
 * time; this panel is the notebook page, the tree is its child.
 *
 * **Owns:** the `wxSearchCtrl` and `SymbolBrowser` (both wx-parented
 * to this panel). **Owned by:** `SideBarManager` (wx-parented to the
 * sidebar notebook).
 *
 * See @ref ui and @ref architecture.
 */
class SymbolBrowserPanel final : public wxPanel {
public:
    NO_COPY_AND_MOVE(SymbolBrowserPanel)

    /// Construct, parented to the sidebar notebook.
    SymbolBrowserPanel(Context& ctx, wxWindow* parent);
    /// Out-of-line so the destructor sees the full `SymbolBrowser` definition.
    ~SymbolBrowserPanel() override;

    /// Forward to the embedded tree — see `SymbolBrowser::setSymbols`.
    void setSymbols(const Document* doc);

    /// Focus the filter box, so the tab opens ready for filter-as-you-type.
    void focusSearch();

private:
    /// Search box changed (typing, search button or cancel) — push the
    /// current query into the tree filter.
    void onSearch(wxCommandEvent& event);

    Context& m_ctx;                 ///< Application context.
    Unowned<wxSearchCtrl> m_search; ///< Live filter input.
    Unowned<SymbolBrowser> m_tree;  ///< Symbol tree.
};

} // namespace fbide
