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
class IntellisenseService;

/**
 * Owns every open `Document`, drives the open / save / close
 * pipelines, and brokers cross-cutting state — the find / replace
 * dialog, the shared on-type `CodeTransformer`, and the background
 * `IntellisenseService`.
 *
 * **Owns:** `m_documents` (vector of `unique_ptr<Document>`),
 * `m_codeTransformer`, `m_intellisense`, `m_findData`.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. The intellisense worker is owned
 * here but lives on its own thread (see @ref analyses).
 * **Field order:** `m_intellisense` is declared *last* so its
 * destructor (which joins the worker) runs *first* — before the
 * documents and transformer it might race with go away.
 *
 * See @ref documents.
 */
class DocumentManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(DocumentManager)

    /// Construct without populating any documents.
    explicit DocumentManager(Context& ctx);
    /// Stop the intellisense worker (declared first so destruction runs first).
    ~DocumentManager() override;

    /// Shared on-type transformer (auto-indent + keyword case). Single
    /// instance reused across all editors — only the active editor drives
    /// it at any given moment, so a shared token buffer is safe.
    [[nodiscard]] auto getCodeTransformer() -> CodeTransformer& { return *m_codeTransformer; }

    /// Create a new empty document and add it as a tab.
    auto newFile(DocumentType type = DocumentType::FreeBASIC) -> Document&;

    /// Open a file. Returns existing document if already open, or nullptr on failure.
    auto openFile(const wxString& filePath) -> Document*;

    /// Resolve and open an `#include` path requested from `origin`.
    /// Search order: relative to `origin` file's directory, then the
    /// compiler's `inc/` folder, then the current working directory.
    /// Returns the opened document, or nullptr if the file cannot be found.
    auto openInclude(const Document& origin, const wxString& includePath) -> Document*;

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

    /// Submit a snapshot for background intellisense parsing. Latest-wins:
    /// any pending submission for any document is replaced. Result lands
    /// asynchronously via EVT_INTELLISENSE_RESULT.
    void submitIntellisense(Document* doc, wxString content);

    /// Cancel any pending or in-flight intellisense work for `doc`. Called
    /// from `closeFile` before erasing the document.
    void cancelIntellisense(const Document* doc);

    /// Handle quit request. Prompts for unsaved docs. Returns true if safe to quit.
    /// If user chooses to save, saves all then returns true.
    /// If user cancels, returns false.
    auto prepareToQuit() -> bool;

    /// Reload a document from disk forcing the given encoding. Prompts to
    /// discard unsaved changes. On success the document's encoding is set
    /// to `encoding` (not re-detected).
    void reloadWithEncoding(Document& doc, TextEncoding encoding);

    /// Reload a document from disk with auto-detected encoding/EOL — same
    /// pipeline as initial open. Prompts the user when the document has
    /// unsaved changes (cancel preserves the buffer).
    void reloadFromDisk(Document& doc);

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
    /// Config-derived default encoding used for freshly opened files.
    [[nodiscard]] auto defaultEncoding() const -> TextEncoding;
    /// Config-derived default EOL mode used for freshly opened files.
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

    /// Find dialog: kick off a find with the latest entered text.
    void onFindDialog(wxFindDialogEvent& event);
    /// Find dialog: repeat the last find from the current caret.
    void onFindDialogNext(wxFindDialogEvent& event);
    /// Replace dialog: replace the current selection then find next.
    void onReplaceDialog(wxFindDialogEvent& event);
    /// Replace dialog: replace every match across the active document.
    void onReplaceAllDialog(wxFindDialogEvent& event);
    /// Find/replace dialog closing — clear the modal pointer.
    void onFindDialogClose(wxFindDialogEvent& event);

    /// Tab-strip context menu — show actions for the right-clicked tab.
    void onTabRightDown(wxAuiNotebookEvent& event);

    /// Intellisense result delivery (worker thread → UI thread).
    void onIntellisenseResult(wxThreadEvent& event);

    Context& m_ctx;                                       ///< Application context.
    wxFindReplaceData m_findData { wxFR_DOWN };           ///< Find/replace dialog state.
    std::vector<std::unique_ptr<Document>> m_documents;   ///< Open documents in tab order.
    std::unique_ptr<CodeTransformer> m_codeTransformer;   ///< Shared on-type transformer.
    /// Declared last so destruction runs first — worker thread stops and
    /// joins before the documents and transformer it might race with go away.
    std::unique_ptr<IntellisenseService> m_intellisense;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
