//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
class Document;

/**
 * The document tab strip — a `wxAuiNotebook` subclass that hosts one
 * tab per open `Document`. Owns the tab UI: add / remove / focus, the
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

    /// Create the notebook as a child of `parent`. `ctx` is borrowed
    /// by reference and used by event handlers to route tab
    /// interactions back into `DocumentManager` and friends.
    DocumentNotebook(wxWindow* parent, Context& ctx);

    /// Append `doc`'s page to the strip. When `select` is true the new
    /// tab also takes focus — matches the historical default for both
    /// `newFile` and `openFile`.
    void addPage(Document& doc, bool select = true);

    /// Drop `doc`'s page from the strip. No-op when the document isn't
    /// currently hosted (already removed, or never added).
    void removePage(Document& doc);

    /// Activate the tab hosting `doc`. No-op when the document isn't
    /// currently hosted.
    void selectDocument(Document& doc);

    /// Refresh the visible tab label from `doc.getTitle()`. Called when
    /// the document is renamed, marked dirty, or its encoding / EOL
    /// flips (the title carries the modified `[*]` prefix).
    void updateTitle(const Document& doc);

    /// The document hosted by the currently-selected tab, or nullptr
    /// when nothing is selected (empty notebook).
    [[nodiscard]] auto activeDocument() const -> Document*;

    /// Walk the open documents looking for the one whose page is
    /// `page`. Returns nullptr when the page doesn't belong to any
    /// tracked document — defensive null on lookup failure since this
    /// is an event-driven path.
    [[nodiscard]] auto documentForPage(const wxWindow* page) const -> Document*;

private:
    /// Locate `doc`'s page index, or `wxNOT_FOUND` when absent.
    [[nodiscard]] auto findIndex(const Document& doc) const -> int;

    /// AUI veto — DocumentManager owns the close pipeline (modified
    /// prompt, intellisense cancel, sidebar refresh). We just route
    /// the close request and let DocumentManager remove the page.
    void onPageClose(wxAuiNotebookEvent& event);
    /// Focus the new active editor, refresh the sidebar's symbol view,
    /// and update the frame title to reflect the focused document.
    void onPageChanged(wxAuiNotebookEvent& event);
    /// Background double-click on the tab strip — convenience shortcut
    /// for "new untitled file".
    void onBgDClick(wxAuiNotebookEvent& event);
    /// Right-click on a tab — activate it, then pop up the per-tab
    /// context menu (Close / Close Others / Show in Browser / etc).
    void onTabRightDown(wxAuiNotebookEvent& event);

    Context& m_ctx;
    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
