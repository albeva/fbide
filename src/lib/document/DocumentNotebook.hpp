//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * The document tab strip — a `wxAuiNotebook` subclass that hosts one
 * tab per open `Document`. Owns the tab UI: open / close / focus, the
 * right-click context menu, and the tab-title bookkeeping.
 *
 * **Owns:** nothing beyond the wx-managed tab pages (each page is a
 * `Document`'s container panel, owned by `DocumentManager` via
 * `unique_ptr<Document>`; the wx tree just borrows the panels).
 * **Owned by:** wx parent (the main frame). `DocumentManager` keeps a
 * non-owning `Unowned<DocumentNotebook>` for typed access.
 * **Lifetime:** constructed by `DocumentManager::createNotebook()`
 * after `UIManager` builds the main frame; destroyed with the frame.
 * **Threading:** UI thread only.
 *
 * Inherits `wxAuiNotebook` directly (rather than proxying one through
 * an `wxEvtHandler`) so call sites can dock it straight into the AUI
 * frame without a `widget()` accessor, and the tab events bind on the
 * class itself.
 */
class DocumentNotebook final : public wxAuiNotebook {
public:
    NO_COPY_AND_MOVE(DocumentNotebook)

    /// Create the notebook as a child of `parent`. Subsequent steps
    /// extend the ctor to accept the `Context&` needed for routing
    /// tab events back into `DocumentManager`.
    explicit DocumentNotebook(wxWindow* parent);
};

} // namespace fbide
