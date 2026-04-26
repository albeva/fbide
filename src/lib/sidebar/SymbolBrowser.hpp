//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/treectrl.h>

namespace fbide {
class Context;
class Document;
class SymbolTable;

/// Sub/Function tree tab. A wxTreeCtrl subclass parented to the sidebar
/// notebook, so the static event table dispatches directly without any
/// PushEventHandler dance — no teardown-order races during frame close.
/// Renders a `Document`'s `SymbolTable`; activating a leaf jumps the active
/// editor to that line and scrolls it to the top of the viewport.
class SymbolBrowser final : public wxTreeCtrl {
public:
    NO_COPY_AND_MOVE(SymbolBrowser)

    SymbolBrowser(Context& ctx, wxWindow* parent);
    ~SymbolBrowser() override = default;

    /// Repopulate from `doc`'s current SymbolTable. `nullptr` (or a doc
    /// with no parsed table yet) clears it. Same `shared_ptr` in a row is
    /// a no-op.
    void setSymbols(const Document* doc);

private:
    void onItemActivated(wxTreeEvent& event);
    void rebuild(const SymbolTable& table);
    void clearTree();

    Context& m_ctx;
    std::shared_ptr<const SymbolTable> m_currentTable;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
