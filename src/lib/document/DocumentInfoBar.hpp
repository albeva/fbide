//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Document.hpp"
#include "ui/controls/FlatButton.hpp"

namespace fbide {
class Context;

/// Non-modal notification bar docked at the top of a document's page, shown
/// when the file changed (or was deleted) on disk while the in-editor buffer
/// has unsaved changes. Offers Reload / Keep actions via flat buttons instead
/// of a focus-stealing modal — so a batch of external edits (e.g. an AI
/// rewriting several files) never stacks a pile of dialogs.
///
/// A plain coloured panel (not `wxInfoBar`): no slide animation, no flicker,
/// and free to host the project's own `FlatButton`s.
///
/// **Owned by:** the `Document`'s container panel (wx parent ownership).
class DocumentInfoBar final : public wxPanel {
public:
    NO_COPY_AND_MOVE(DocumentInfoBar)

    DocumentInfoBar(wxWindow* parent, Context& ctx, Document& doc);
    ~DocumentInfoBar() override = default;

    /// Show the "changed on disk, you have unsaved edits" conflict bar.
    void showConflict();
    /// Show the "deleted on disk" bar.
    void showDeleted();
    /// Hide the bar and re-layout the page.
    void dismiss();

private:
    /// Reload from disk, discarding the in-editor changes (keeps undo).
    void onReload(wxCommandEvent& event);
    /// Dismiss: keep the in-editor version (re-baseline so we stop nagging).
    void onKeep(wxCommandEvent& event);

    /// Apply the message + button visibility for the current state, then show.
    void present(const wxString& message);

    Context& m_ctx;
    Document& m_doc;

    Unowned<wxStaticText> m_message;
    Unowned<FlatButton> m_reload; ///< Conflict only: reload from disk.
    Unowned<FlatButton> m_keep;   ///< Dismiss (keep the in-editor version).

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
