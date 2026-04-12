//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Document.hpp"
#include "DocumentType.hpp"

namespace fbide {
class Context;

/// Manages open documents and their notebook tabs.
/// Extends wxEvtHandler to receive find/replace dialog events.
class DocumentManager final : public wxEvtHandler {
public:
    explicit DocumentManager(Context& ctx);

    /// Create a new empty document and add it as a tab.
    auto createNew(DocumentType type = DocumentType::FreeBASIC) -> Document&;

    /// Open a file. Returns existing document if already open, or nullptr on failure.
    auto open(const wxString& filePath) -> Document*;

    /// Show open file dialog and open selected files.
    void openWithDialog();

    /// Save a document. Shows save dialog if untitled. Returns false if cancelled.
    auto save(Document& doc) const -> bool;

    /// Save a document with a new name. Returns false if cancelled.
    auto saveAs(Document& doc) const -> bool;

    /// Save all modified documents. Returns false if any save was cancelled.
    auto saveAll() const -> bool;

    /// Close a document. Returns false if user cancelled (unsaved changes).
    auto close(Document& doc) -> bool;

    /// Close all documents. Returns false if user cancelled.
    auto closeAll() -> bool;

    /// Handle quit request. Prompts for unsaved docs. Returns true if safe to quit.
    /// If user chooses to save, saves all then returns true.
    /// If user cancels, returns false.
    auto prepareToQuit() -> bool;

    /// Get currently active document (selected tab), or nullptr if none.
    [[nodiscard]] auto getActive() const -> Document*;

    /// Get document count.
    [[nodiscard]] auto getCount() const -> size_t { return m_documents.size(); }

    /// Find document by file path. Returns nullptr if not found.
    [[nodiscard]] auto findByPath(const wxString& path) const -> Document*;

    /// Get number of modified documents.
    [[nodiscard]] auto getModifiedCount() const -> size_t;

    /// Update the tab title for the active document.
    void updateActiveTabTitle() const;

    /// Find document by its editor widget.
    [[nodiscard]] auto findByEditor(const wxWindow* editor) const -> Document*;

    /// Show the Find dialog (pre-filled from active editor).
    void showFind();

    /// Show the Replace dialog (pre-filled from active editor).
    void showReplace();

    /// Repeat the last find operation.
    void findNext();

    /// Show the Goto Line dialog.
    void gotoLine();

private:

    /// Find notebook page index for a document.
    [[nodiscard]] auto findPageIndex(const Document& doc) const -> int;

    /// Update notebook tab title for a document.
    void updateTabTitle(const Document& doc) const;

    /// Get the notebook from UIManager.
    [[nodiscard]] auto getNotebook() const -> wxAuiNotebook*;

    /// Open find or replace dialog.
    void showFindDialog(bool replace);

    // Find/Replace dialog events
    void onFindDialog(wxFindDialogEvent& event);
    void onFindDialogNext(wxFindDialogEvent& event);
    void onReplaceDialog(wxFindDialogEvent& event);
    void onReplaceAllDialog(wxFindDialogEvent& event);
    void onFindDialogClose(wxFindDialogEvent& event);

    Context& m_ctx;
    wxFindReplaceData m_findData { wxFR_DOWN };
    std::vector<std::unique_ptr<Document>> m_documents;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
