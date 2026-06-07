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

/**
 * Find / Replace / Goto Line for the focused editor.
 *
 * Owns the `wxFindReplaceData` that carries search state between
 * dialog invocations and routes `wxFindDialogEvent` events back into
 * the active editor's find / replace pipeline. Lives outside
 * `DocumentManager` because every operation here is editor-bound and
 * never touches the document collection.
 *
 * **Owns:** the `wxFindReplaceData` state. Dialog widgets are
 * wx-parented to the main frame and clean themselves up on close.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 */
class EditorSearchService final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(EditorSearchService)

    /// Construct without showing any dialog. `ctx` is borrowed by
    /// reference and used to reach the active document and the main
    /// frame each time a dialog is needed.
    explicit EditorSearchService(Context& ctx);

    /// Show the Find dialog, pre-filled from the active editor's
    /// current selection or word-under-cursor.
    void showFind();

    /// Show the Replace dialog, pre-filled the same way.
    void showReplace();

    /// Repeat the last find against the active editor. Falls back to
    /// `showFind()` when no previous search text exists.
    void findNext();

    /// Prompt the user for a line number / `line:col` / `e`-for-end
    /// expression and jump to it in the active editor.
    void gotoLine();

private:
    /// Shared dialog launcher for Find / Replace.
    void showFindDialog(bool replace);

    void onFindDialog(wxFindDialogEvent& event);
    void onFindDialogNext(wxFindDialogEvent& event);
    void onReplaceDialog(wxFindDialogEvent& event);
    void onReplaceAllDialog(wxFindDialogEvent& event);
    void onFindDialogClose(wxFindDialogEvent& event);

    Context& m_ctx;
    wxFindReplaceData m_findData { wxFR_DOWN };
    Unowned<wxFindReplaceDialog> m_findDialog; ///< Live modeless find/replace dialog, or null when none is open.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
