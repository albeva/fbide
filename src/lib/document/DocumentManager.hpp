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
#include "TextEncoding.hpp"

namespace fbide {
class Context;
class CodeTransformer;

/// Manages open documents and their notebook tabs.
/// Extends wxEvtHandler to receive find/replace dialog events.
class DocumentManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(DocumentManager)

    explicit DocumentManager(Context& ctx);
    ~DocumentManager() override;

    /// Shared on-type transformer (auto-indent + keyword case). Single
    /// instance reused across all editors — only the active editor drives
    /// it at any given moment, so a shared token buffer is safe.
    [[nodiscard]] auto getCodeTransformer() -> CodeTransformer& { return *m_codeTransformer; }

    /// Create a new empty document and add it as a tab.
    auto newFile(DocumentType type = DocumentType::FreeBASIC) -> Document&;

    /// Open a file. Returns existing document if already open, or nullptr on failure.
    auto openFile(const wxString& filePath) -> Document*;

    /// Show open file dialog and open selected files.
    void openFile();

    /// Save a document. Shows save dialog if untitled. Returns false if cancelled.
    auto saveFile(Document& doc) const -> bool;

    /// Save a document with a new name. Returns false if cancelled.
    auto saveFileAs(Document& doc) const -> bool;

    /// Save all modified documents. Returns false if any save was cancelled.
    auto saveAllFiles() const -> bool;

    /// Close a document. Returns false if user cancelled (unsaved changes).
    auto closeFile(Document& doc) -> bool;

    /// Close all documents. Returns false if user cancelled.
    auto closeAllFiles() -> bool;

    /// Close every document except `keep`. Returns false if user cancelled.
    auto closeOtherFiles(const Document& keep) -> bool;

    /// Bind tab-strip events on the document notebook. Call from UIManager
    /// once the notebook exists.
    void attachNotebook();

    /// Refresh enable/disable state of edit commands (Undo, Redo, Cut, Copy,
    /// Paste, SelectAll) from the active editor. Called whenever the editor
    /// state may have changed (focus, modification, selection).
    void syncEditCommands();

    /// Handle quit request. Prompts for unsaved docs. Returns true if safe to quit.
    /// If user chooses to save, saves all then returns true.
    /// If user cancels, returns false.
    auto prepareToQuit() -> bool;

    /// Reload a document from disk forcing the given encoding. Prompts to
    /// discard unsaved changes. On success the document's encoding is set
    /// to `encoding` (not re-detected).
    void reloadWithEncoding(Document& doc, TextEncoding encoding);

    /// Get currently active document (selected tab), or nullptr if none.
    [[nodiscard]] auto getActive() const -> Document*;

    /// Set documents' editor to be currently active editive one
    void setActive(Document* document);

    /// Get document count.
    [[nodiscard]] auto getCount() const -> size_t { return m_documents.size(); }

    /// Find document by file path. Returns nullptr if not found.
    [[nodiscard]] auto findByPath(const wxString& path) const -> Document*;

    /// Get number of modified documents.
    [[nodiscard]] auto getModifiedCount() const -> size_t;

    /// Update the tab title for the active document.
    void updateActiveTabTitle() const;

    /// Check if a document pointer is still valid (not closed).
    [[nodiscard]] auto contains(const Document* doc) const -> bool;

    /// Find document by its editor widget.
    [[nodiscard]] auto findByEditor(const wxWindow* editor) const -> Document*;

    /// Iterate all open documents (ordering matches tab order at creation).
    [[nodiscard]] auto getDocuments() const
        -> std::span<const std::unique_ptr<Document>> { return m_documents; }

    /// Show the Find dialog (pre-filled from active editor).
    void showFind();

    /// Show the Replace dialog (pre-filled from active editor).
    void showReplace();

    /// Repeat the last find operation.
    void findNext();

    /// Show the Goto Line dialog.
    void gotoLine();

private:
    /// Config-derived defaults used to seed encoding/EOL detection for
    /// freshly opened files before a Document exists.
    [[nodiscard]] auto defaultEncoding() const -> TextEncoding;
    [[nodiscard]] auto defaultEolMode() const -> EolMode;

    /// Find notebook page index for a document.
    [[nodiscard]] auto findPageIndex(const Document& doc) const -> int;

    /// Update notebook tab title for a document.
    void updateTabTitle(const Document& doc) const;

    /// Get the notebook from UIManager.
    [[nodiscard]] auto getNotebook() const -> wxAuiNotebook*;

    /// Open find or replace dialog.
    void showFindDialog(bool replace);

    /// If the saved file is a loaded IDE config, reload it and refresh
    /// editor settings (same chain as SettingsDialog::applyChanges).
    void reloadConfigIfMatches(const wxString& path) const;

    // Find/Replace dialog events
    void onFindDialog(wxFindDialogEvent& event);
    void onFindDialogNext(wxFindDialogEvent& event);
    void onReplaceDialog(wxFindDialogEvent& event);
    void onReplaceAllDialog(wxFindDialogEvent& event);
    void onFindDialogClose(wxFindDialogEvent& event);

    // Tab-strip context menu
    void onTabRightDown(wxAuiNotebookEvent& event);

    Context& m_ctx;
    wxFindReplaceData m_findData { wxFR_DOWN };
    std::vector<std::unique_ptr<Document>> m_documents;
    std::unique_ptr<CodeTransformer> m_codeTransformer;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
