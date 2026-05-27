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
#include "workspace/Project.hpp"

namespace fbide {
class ConfigManager;
class Document;
class DocumentTypeChangedEvent;
class Editor;

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
        // wxEvent::Clone contract: caller (wxWidgets event loop) takes ownership.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        return new DocumentTypeChangedEvent(*this);
    }

private:
    Document* m_doc;
    DocumentType m_previous;
};

/**
 * One open file (or untitled buffer) — the data model: file path,
 * type, encoding, EOL, mod-time.
 *
 * Pairs with an `EditorPanel` view that hosts the actual `Editor`
 * widget + the optional minimap; `Document` keeps a non-owning back
 * link to the panel. The `EditorPanel` is wx-parented to the
 * notebook — `Document` does not own it.
 *
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
    /// Where this document's on-disk identity lives. When the document
    /// is unbound from any project, the path is stored directly in
    /// `m_source` as a `std::filesystem::path` (empty for untitled).
    /// When the document is bound to a project, the path lives on the
    /// project's `Node` and `m_source` carries a non-owning `Node*`
    /// pinned by the project's `unique_ptr` storage; the `m_project`
    /// back-link names the owning project. Invariant held by
    /// `bindToProject` / `unbindFromProject`: `m_project != nullptr`
    /// iff `m_source` holds `Project::Node*`.
    using Source = std::variant<std::filesystem::path, Project::Node*>;

    NO_COPY_AND_MOVE(Document)

    /// Create a new document. View-less by default — call
    /// `attachView()` (typically driven by `EditorPanel`'s ctor) to
    /// pair the document with a hosted editor. `sink`, if non-null,
    /// receives `EVT_DOCUMENT_TYPE_CHANGED` whenever `setType`
    /// commits a transition (typically the `DocumentManager`).
    /// `config` supplies the encoding/EOL defaults and the locale
    /// table used to format the "Untitled" tab title.
    explicit Document(ConfigManager& config, DocumentType type = DocumentType::FreeBASIC, wxEvtHandler* sink = nullptr);

    /// Publish the view back-link. `view` is the wxWindow that hosts
    /// the document on screen (a notebook page); `editor` is its
    /// editor widget if the view hosts one (`EditorPanel` does;
    /// future image/markdown panels may not). When an editor is
    /// supplied the document's current EOL is pushed into it so a
    /// pre-set encoding/EOL choice is reflected before any text
    /// loads. Typically called from the view's constructor.
    void attachView(wxWindow* view, Editor* editor = nullptr);

    /// Drop both view back-links. The view's destructor calls this so
    /// wx-parent-driven teardown (notebook page close) leaves the
    /// document with clean `nullptr` slots.
    void detachView();

    /// True when a view is currently attached.
    [[nodiscard]] auto hasView() const -> bool { return m_view != nullptr; }

    /// Get the file path. Empty if untitled. Returned by value because
    /// when the document is project-bound the path lives on the project's
    /// node and would otherwise require returning a reference to a
    /// potentially-absent `optional` slot — by-value is cheap for
    /// `std::filesystem::path` and avoids that hazard. Callers that
    /// hand it to a wx API should wrap with `toWxString(...)`.
    [[nodiscard]] auto getFilePath() const -> std::filesystem::path;

    /// Set the file path. When the document is project-bound, the path
    /// mutation is forwarded to `Project::setNodePath` so the project's
    /// path index stays in sync.
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

    /// The editor widget hosted by this document's view, or `nullptr`
    /// when the document has no view, or its view doesn't host an
    /// editor (a future image/markdown panel, for example).
    [[nodiscard]] auto getEditor() -> Editor* { return m_editor.get(); }
    /// Const overload of `getEditor`.
    [[nodiscard]] auto getEditor() const -> const Editor* { return m_editor.get(); }

    /// The notebook page hosting this document, as a generic
    /// `wxWindow*`. Used by the notebook for tab management — concrete
    /// view kind (`EditorPanel`, etc.) stays opaque at this layer.
    [[nodiscard]] auto getView() -> wxWindow* { return m_view.get(); }
    /// Const overload of `getView`.
    [[nodiscard]] auto getView() const -> const wxWindow* { return m_view.get(); }

    /// Enable or disable the minimap. Effective visibility also depends
    /// on the page being wide enough — see `EditorPanel::updateMinimapVisibility`.
    void showMinimap(bool enabled);

    /// Is this a new (never saved) document?
    [[nodiscard]] auto isNew() const -> bool { return getFilePath().empty(); }

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

    /// Update document controls settings. Forwards to the view when
    /// the view is an `EditorPanel`; no-op for other view kinds.
    void updateSettings();

    /// The project this document is bound to, or `nullptr` when it
    /// lives outside any project (non-FreeBASIC docs in the current
    /// phase; eventually any doc the user hasn't added to a persistent
    /// project). Lifetime: the project always outlives the bound
    /// document — the `WorkspaceManager` teardown protocol calls
    /// `unbindFromProject` before destroying a project.
    [[nodiscard]] auto getProject() const -> Project* { return m_project; }

    /// Bind this document to a project under the given node. The path
    /// currently held in `m_source` is **not** propagated here — the
    /// caller is expected to have stored it on the project's node
    /// first (typically via `Project::addFile`). After this call,
    /// `getFilePath()` resolves through the project.
    void bindToProject(Project* project, Project::Node* node);

    /// Detach this document from its project, atomically copying the
    /// path out of the project's node back into `m_source` so
    /// `getFilePath()` keeps returning the same value either side of
    /// the transition. Safe to call when already unbound (no-op).
    void unbindFromProject();

private:
    ConfigManager& m_config;                   ///< Source of encoding/EOL defaults and the locale table.
    Source m_source;                           ///< Path (unbound) or node ID (project-bound); see `Source`.
    Project* m_project = nullptr;              ///< Owning project; non-null iff `m_source` holds `Project::Node*`.
    DocumentType m_type;                       ///< Document type — drives lexer + theme dispatch.
    bool m_typeOverridden = false;             ///< True when the user explicitly picked the type.
    Unowned<wxWindow> m_view;                  ///< Generic view back-link — wx-parented to the notebook.
    Unowned<Editor> m_editor;                  ///< Typed editor pointer when the view hosts one; null otherwise.
    std::filesystem::file_time_type m_modTime; ///< Last on-disk mtime — backs `checkExternalChange`.
    TextEncoding m_encoding;                   ///< Bytes-to-text codec used on save.
    EolMode m_eolMode;                         ///< Line-ending convention applied on save.
    /// Set when encoding is changed; cleared on save. OR'd with the
    /// view's modify flag in isModified() so encoding-only edits still
    /// show as dirty.
    bool m_metaModified = false;
    wxEvtHandler* m_sink = nullptr; ///< Sink for `EVT_DOCUMENT_TYPE_CHANGED`; null = no observer.
};

} // namespace fbide
