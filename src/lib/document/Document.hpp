//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "DocumentType.hpp"
#include "TextEncoding.hpp"
#include "analyses/symbols/SymbolTable.hpp"

namespace fbide {
class Context;
class Document;
class DocumentTypeChangedEvent;
class Editor;
class EditorPanel;
class ProjectNode;

/// Fired by `Document::setType` after the type transition is committed
/// and the view (if any) has been updated. The event carries the
/// document pointer and the previous type so subscribers can detect
/// transitions in either direction (entering / leaving FreeBASIC).
/// Dispatched synchronously to the sink wxEvtHandler passed to the
/// document's constructor — view-less / sink-less documents fire
/// nothing.
wxDECLARE_EVENT(EVT_DOCUMENT_TYPE_CHANGED, DocumentTypeChangedEvent);

class DocumentTypeChangedEvent final : public wxEvent {
public:
    DocumentTypeChangedEvent(Document* doc, DocumentType previous)
    : wxEvent(0, EVT_DOCUMENT_TYPE_CHANGED)
    , m_doc(doc)
    , m_previous(previous) {}

    [[nodiscard]] auto getDocument() const -> Document* { return m_doc; }
    [[nodiscard]] auto getPreviousType() const -> DocumentType { return m_previous; }

    [[nodiscard]] auto Clone() const -> wxEvent* override {
        return new DocumentTypeChangedEvent(*this);
    }

private:
    Document* m_doc;
    DocumentType m_previous;
};

/**
 * One open file (or untitled buffer) — the data model: file path,
 * type, encoding, EOL, mod-time, latest symbol table.
 *
 * Pairs with an `EditorPanel` view that hosts the actual `Editor`
 * widget + the optional minimap; `Document` keeps a non-owning back
 * link to the panel and forwards editor-shaped queries through it
 * for the convenience of call sites that today reach for
 * `doc->getEditor()`. Later phases drop the construct-time view
 * requirement and let documents exist without an open tab (project
 * tree).
 *
 * **Owns:** the model state listed below plus the
 * `shared_ptr<const SymbolTable>` published by intellisense. Does
 * NOT own the `EditorPanel` view in the wx sense — the panel is
 * wx-parented to the notebook.
 * **Owned by:** `DocumentManager` via `unique_ptr<Document>`.
 *
 * `m_metaModified` tracks encoding/EOL changes separately from the
 * editor's own dirty bit so metadata-only edits round-trip through
 * "modified" → "saved" the same way text edits do.
 *
 * See @ref documents.
 */
class Document final {
public:
    NO_COPY_AND_MOVE(Document)

    /// Create a new document. View-less by default — call
    /// `attachView()` (typically driven by `EditorPanel`'s ctor) to
    /// pair the document with a hosted editor. `sink`, if non-null,
    /// receives `EVT_DOCUMENT_TYPE_CHANGED` whenever `setType`
    /// commits a transition (typically the `DocumentManager`).
    explicit Document(Context& ctx, DocumentType type = DocumentType::FreeBASIC, wxEvtHandler* sink = nullptr);

    /// Attach `panel` as this document's hosting view. Pushes the
    /// document's current EOL state into the panel's editor so a
    /// pre-set encoding/EOL choice is reflected before any text
    /// loads. Typically called from `EditorPanel`'s constructor.
    void attachView(EditorPanel* panel);

    /// Drop the view back-link. `EditorPanel`'s destructor calls this
    /// so wx-parent-driven destruction of the panel (notebook page
    /// close) leaves the document with a clean `nullptr` slot.
    void detachView();

    /// True when a view is currently attached.
    [[nodiscard]] auto hasView() const -> bool { return m_panel != nullptr; }

    /// Get the file path. Empty if untitled. Returned as `std::filesystem::path`
    /// — callers that hand it to a wx API should wrap with `toWxString(...)`.
    [[nodiscard]] auto getFilePath() const -> const std::filesystem::path& { return m_filePath; }

    /// Set the file path.
    void setFilePath(const std::filesystem::path& path);

    /// Get display title for tab (filename or "Untitled").
    [[nodiscard]] auto getTitle() const -> wxString;

    /// Title for the application frame caption. Saved documents
    /// surface the full filesystem path so the user can tell similarly
    /// named files apart at a glance; untitled documents fall back to
    /// the tab title. Centralised here so the chat-side tab-change
    /// handler and the document-side save/reload helpers agree on
    /// the rule without each restating it.
    [[nodiscard]] auto getFrameTitle() const -> wxString;

    /// Get document type.
    [[nodiscard]] auto getType() const -> DocumentType { return m_type; }

    /// Override the document type (user picked from the status bar menu).
    /// Sticky across Save / Save As until the document is closed — the
    /// override flag is per-instance, only persisted via FileSession.
    /// Reapplies editor settings (lexer + theme), then posts
    /// `EVT_DOCUMENT_TYPE_CHANGED` to the sink so subscribers
    /// (DocumentManager) can run the cross-cutting side effects
    /// (intellisense submit/cancel, sidebar refresh) without
    /// `Document` having to know about them.
    void setType(DocumentType type);

    /// True when the type was set explicitly via `setType` rather than
    /// derived from the file path.
    [[nodiscard]] auto isTypeOverridden() const -> bool { return m_typeOverridden; }

    /// The editor widget hosted by this document's view. Forwarded
    /// from `EditorPanel` for the convenience of code that already
    /// holds a `Document*`. Will return `nullptr` in a future phase
    /// where documents may exist without an attached view.
    [[nodiscard]] auto getEditor() -> Editor*;
    /// Const overload of `getEditor`.
    [[nodiscard]] auto getEditor() const -> const Editor*;

    /// Get the notebook page — the `EditorPanel` instance itself
    /// (which IS a `wxPanel`). Returned as `wxWindow*` so the
    /// notebook can dock it without knowing the concrete view kind.
    [[nodiscard]] auto getPage() -> wxWindow*;
    /// Const overload of `getPage`.
    [[nodiscard]] auto getPage() const -> const wxWindow*;

    /// Enable or disable the minimap. Effective visibility also depends
    /// on the page being wide enough — see `EditorPanel::updateMinimapVisibility`.
    void showMinimap(bool enabled);

    /// Path of the most recently compiled executable.
    [[nodiscard]] auto getCompiledFile() const -> wxString { return m_compiledFile; }

    /// Record the path of the freshly compiled executable.
    void setCompiledPath(const wxString& path) { m_compiledFile = path; }

    /// Get the keyword at the cursor position.
    /// For FreeBASIC documents, includes '#' prefix for preprocessor directives.
    /// Returns empty string if no word at cursor.
    [[nodiscard]] auto getKeywordAtCursor() const -> wxString;

    /// Is this a new (never saved) document?
    [[nodiscard]] auto isNew() const -> bool { return m_filePath.empty(); }

    /// Get the text encoding used on save.
    [[nodiscard]] auto getEncoding() const -> TextEncoding { return m_encoding; }

    /// Change encoding. Does not mutate buffer contents — only affects
    /// bytes written on next save. Marks document dirty.
    void setEncoding(TextEncoding encoding);

    /// Get the line-ending mode.
    [[nodiscard]] auto getEolMode() const -> EolMode { return m_eolMode; }

    /// Change EOL mode. Rewrites buffer line endings via ConvertEOLs and
    /// applies to wxSTC for future inserts. Marks document dirty.
    void setEolMode(EolMode mode);

    /// Is the document modified? Combines the meta-dirty flag with
    /// the view's editor buffer-dirty state when a view is attached.
    [[nodiscard]] auto isModified() const -> bool;

    /// Mark the document as clean — clears the editor's dirty flag
    /// (via the view's save point) and the encoding-change meta-dirty
    /// flag. Call after a successful save, load, or reload.
    void markSaved();

    /// Check if file was modified externally since last load/save.
    [[nodiscard]] auto checkExternalChange() const -> bool;

    /// Update stored modification time from file on disk.
    void updateModTime();

    /// Latest symbol table produced by IntellisenseService for this document.
    /// May be null until the first parse completes.
    [[nodiscard]] auto getSymbolTable() const
        -> std::shared_ptr<const SymbolTable> { return m_symbolTable; }

    /// Set the latest symbol table. Called by DocumentManager from the
    /// IntellisenseService result handler on the UI thread.
    void setSymbolTable(std::shared_ptr<const SymbolTable> table) {
        m_symbolTable = std::move(table);
    }

    /// Update document controls settings. Forwards to the view.
    void updateSettings();

    /// The project tree node this document is backed by, if any.
    /// Stays `nullptr` until the project layer attaches one — the
    /// document layer itself never dereferences this back-link;
    /// project code walks documents → nodes for tree refreshes.
    [[nodiscard]] auto projectNode() const -> ProjectNode* { return m_projectNode; }

    /// Set (or clear, with `nullptr`) the project-node back-link.
    /// Called from the project layer when a document is bound to a
    /// tree entry; ownership lives on the project side.
    void setProjectNode(ProjectNode* node) { m_projectNode = node; }

private:
    Context& m_ctx;                            ///< Application context.
    wxString m_compiledFile;                   ///< Path of the most recently compiled executable.
    std::filesystem::path m_filePath;          ///< Absolute path on disk; empty for new documents.
    DocumentType m_type;                       ///< Document type — drives lexer + theme dispatch.
    bool m_typeOverridden = false;             ///< True when the user explicitly picked the type.
    Unowned<EditorPanel> m_panel;              ///< View back-link — wx-parented to the notebook.
    std::filesystem::file_time_type m_modTime; ///< Last on-disk mtime — backs `checkExternalChange`.
    TextEncoding m_encoding;                   ///< Bytes-to-text codec used on save.
    EolMode m_eolMode;                         ///< Line-ending convention applied on save.
    /// Set when encoding is changed; cleared on save. OR'd with the
    /// view's modify flag in isModified() so encoding-only edits still
    /// show as dirty.
    bool m_metaModified = false;
    // REVIEW: I think SymbolTable is really property of Editor, not the document itself.
    std::shared_ptr<const SymbolTable> m_symbolTable; ///< Latest intellisense result for this document.
    wxEvtHandler* m_sink = nullptr;                   ///< Sink for `EVT_DOCUMENT_TYPE_CHANGED`; null = no observer.
    Unowned<ProjectNode> m_projectNode = nullptr;     ///< Project-tree back-link; populated by project code.
};

} // namespace fbide
