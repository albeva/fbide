//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Document.hpp"
#include "DocumentIO.hpp"
#include "DocumentType.hpp"
#include "TextEncoding.hpp"

namespace fbide {
class Context;
class CodeTransformer;
class DocumentNotebook;

/**
 * Drives the open / save / close pipelines and the document notebook,
 * and brokers cross-cutting state — the shared on-type `CodeTransformer`
 * and the background `IntellisenseService`.
 *
 * Documents are **not** owned here — every `Document` belongs to a project
 * (the shared `EphemeralProject` for standalone files, a persistent
 * `Project` for project members). This class owns only the `CodeTransformer`
 * and manages editors / tabs over those documents.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. The shared `IntellisenseService` lives
 * on `WorkspaceManager` (see @ref analyses); `DocumentManager` only
 * forwards `submitIntellisense` / `cancelIntellisense` / pool prune
 * calls through it.
 *
 * See @ref documents.
 */
class DocumentManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(DocumentManager)

    /// Construct without populating any documents.
    explicit DocumentManager(Context& ctx);
    /// Out-of-line so the destructor sees the full `CodeTransformer`
    /// definition (only forward-declared in this header).
    ~DocumentManager() override;

    /// Shared on-type transformer (auto-indent + keyword case). Single
    /// instance reused across all editors — only the active editor drives
    /// it at any given moment, so a shared token buffer is safe.
    [[nodiscard]] auto getCodeTransformer() -> CodeTransformer& { return *m_codeTransformer; }

    /// Create a new empty document and add it as a tab.
    auto newFile(DocumentType type = DocumentType::FreeBASIC) -> Document&;

    /// Open a standalone document file: dedup against open tabs, create the
    /// `Document` (owned by the shared ephemeral project) plus its editor, and
    /// load the content. Session (`.fbs`) / project (`.fbp`) files are routed
    /// elsewhere by `WorkspaceManager::openFile` before reaching here. Returns
    /// the document, or nullptr on failure.
    auto openDocument(const std::filesystem::path& filePath) -> Document*;

    /// Ensure `doc` has an editor + tab and focus it: if already open, select
    /// its tab; otherwise create the editor, load its on-disk content (for
    /// saved documents), set the type-change sink, and add the tab. The shared
    /// path for double-clicking a project file and reopening a project member.
    void openEditorFor(Document& doc);

    /// Tear down `doc`'s editor + tab *without* destroying the document or
    /// running the close dispatch — used when closing a persistent project,
    /// whose nodes still own the documents (`WorkspaceManager::closeProject`).
    void closeEditor(Document& doc);

    /// Resolve and open an `#include` path requested from `origin`.
    /// Search order mirrors fbc's: the `origin` file's directory, then the
    /// `-i` directories from its active configuration's compile command,
    /// then that configuration's compiler `inc/` folder, then the current
    /// working directory as a fallback.
    /// Returns the opened document, or nullptr if the file cannot be found.
    auto openInclude(const Document& origin, const wxString& includePath) -> Document*;

    /// Save a document. Shows save dialog if untitled. Returns false if cancelled.
    auto saveFile(Document& doc) -> bool;

    /// Save a document with a new name. Returns false if cancelled.
    /// May reload another open document if its on-disk file is overwritten.
    auto saveFileAs(Document& doc) -> bool;

    /// Save all modified documents. Returns false if any save was cancelled.
    auto saveAllFiles() -> bool;

    /// Close a document. Returns false if user cancelled (unsaved changes).
    auto closeFile(Document& doc) -> bool;

    /// Close all documents. Returns false if user cancelled.
    auto closeAllFiles() -> bool;

    /// Close every document except `keep`. Returns false if user cancelled.
    auto closeOtherFiles(const Document& keep) -> bool;

    /// Two-phase init — `UIManager::createLayout` calls this once the
    /// main frame exists. Constructs the `DocumentNotebook` widget
    /// (wx-parented to `parent`, owned by the wx tree) and stashes
    /// the non-owning pointer for subsequent `notebook()` lookups.
    /// Returns a reference so the caller can dock the new widget
    /// without a follow-up lookup.
    auto createNotebook(wxWindow* parent) -> DocumentNotebook&;

    /// The document tab strip. Must be called after `createNotebook`.
    [[nodiscard]] auto notebook() -> DocumentNotebook& { return *m_notebook; }
    /// Const overload of `notebook`.
    [[nodiscard]] auto notebook() const -> const DocumentNotebook& { return *m_notebook; }

    /// Refresh enable/disable state of edit commands (Undo, Redo, Cut, Copy,
    /// Paste, SelectAll) from the active editor. Called whenever the editor
    /// state may have changed (focus, modification, selection).
    void syncEditCommands();

    /// Submit a snapshot for background intellisense parsing. Latest-wins:
    /// any pending submission for any document is replaced. Result lands
    /// asynchronously via EVT_INTELLISENSE_RESULT.
    void submitIntellisense(Document* doc, const wxString& content);

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

    /// Number of open documents (tabs).
    [[nodiscard]] auto getCount() const -> size_t;

    /// Find document by file path. Returns nullptr if not found.
    [[nodiscard]] auto findByPath(const wxString& path) const -> Document*;

    /// fs::path overload — used by internal callers that already operate
    /// on `std::filesystem::path`. Identical semantics to the `wxString`
    /// overload but skips one round-trip conversion.
    [[nodiscard]] auto findByPath(const std::filesystem::path& path) const -> Document*;

    /// Get number of modified documents.
    [[nodiscard]] auto getModifiedCount() const -> size_t;

    /// Update the tab title for the active document.
    void updateActiveTabTitle() const;

    /// Check if a document pointer is still valid (not closed).
    [[nodiscard]] auto contains(const Document* doc) const -> bool;

    /// Find document by its editor widget.
    [[nodiscard]] auto findByEditor(const wxWindow* editor) const -> Document*;

    /// Show or hide the minimap on every open document.
    void setMinimapVisible(bool visible);

private:
    /// The currently open documents (those with an editor/tab).
    [[nodiscard]] auto openDocuments() const -> std::vector<Document*>;

    /// Config-derived default encoding used for freshly opened files.
    [[nodiscard]] auto defaultEncoding() const -> TextEncoding;
    /// Config-derived default EOL mode used for freshly opened files.
    [[nodiscard]] auto defaultEolMode() const -> EolMode;

    /// Refresh the tab text + frame title for `doc`. Combined helper
    /// because the historical `updateTabTitle` did both; preserved as
    /// one entry point so callers don't have to re-derive the title
    /// derivation rule.
    void refreshTitleFor(const Document& doc) const;

    /// When the saved file is a loaded IDE config (theme / shortcuts /
    /// keywords / etc.), prompt the user to restart FBIde so the
    /// changes take effect. Mirrors the language-change restart flow.
    void promptRestartIfConfig(const std::filesystem::path& path) const;

    /// Side-effect bundle fired when a document's type changes —
    /// EVT_DOCUMENT_TYPE_CHANGED handler. Submits / cancels
    /// intellisense and refreshes the sidebar so the document model
    /// can stay agnostic about who's listening.
    void onDocumentTypeChanged(DocumentTypeChangedEvent& event);

    /// Surface a `DocumentIO::SaveResult` failure to the user as a
    /// localised error message. No-op on `Success`.
    void reportSaveFailure(DocumentIO::SaveResult result, TextEncoding encoding) const;

    /// Push loaded text into `doc`'s editor, applying `eol` (sets STC's
    /// EOL mode and converts existing line endings to match). Suppresses
    /// on-type transforms during the SetText and clears the undo buffer.
    /// Used by `openFile`, `reloadWithEncoding`, and `reloadFromDisk` —
    /// each picks the EOL appropriate to its intent (loaded vs. existing).
    void loadFile(Document& doc, const wxString& text, EolMode eol) const;

    /// Intellisense result delivery (worker thread → UI thread).
    void onIntellisenseResult(wxThreadEvent& event);

    Context& m_ctx;                                     ///< Application context.
    Unowned<DocumentNotebook> m_notebook;               ///< Tab strip — wx-parented to the frame; created by `createNotebook`.
    std::unique_ptr<CodeTransformer> m_codeTransformer; ///< Shared on-type transformer.
};

} // namespace fbide
