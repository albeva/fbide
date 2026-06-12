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

/// Which slice of a document's persistable attributes `set/loadSessionAttributes`
/// reads or writes — chosen by the persistence target so each value lands in
/// the right file.
enum class SessionScope : std::uint8_t {
    /// Everything (including the pinned compiler configuration). Standalone
    /// files store all their state in the `.fbs` session — their only
    /// persistence.
    Ephemeral,
    /// Per-user runtime UI state only — scroll, caret, folds. Lives in the
    /// project's `.fbide/session.ini` (safe to delete / gitignore).
    Session,
    /// Project-meaningful, version-controlled state — encoding, EOL, and the
    /// type override. Lives in the `.fbp` so the project builds without the
    /// session file.
    Project,
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
    /// Where this document's on-disk identity lives. `m_source` holds a
    /// `std::filesystem::path` (empty for untitled) for every document
    /// except those bound to a **persistent** `Project`, where it instead
    /// carries a non-owning `Project::Node*` and the path lives on that
    /// node. Standalone documents (owned by the shared `EphemeralProject`)
    /// keep their own path here. Invariant held by `bindToProject`:
    /// `m_source` holds `Project::Node*` iff bound to a persistent `Project`.
    using Source = std::variant<std::filesystem::path, Project::Node*>;

    NO_COPY_AND_MOVE(Document)

    /// Pending external-change state, surfaced via the page's info bar
    /// until the user resolves it (Reload or Keep).
    enum class ExternalChange : std::uint8_t {
        None,     ///< In sync with disk.
        Conflict, ///< Changed on disk while the buffer has unsaved edits.
        Deleted,  ///< Removed from disk while still open.
    };

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

    /// Set (or clear) the sink that receives `EVT_DOCUMENT_TYPE_CHANGED`.
    /// `DocumentManager` sets itself as the sink when it attaches an editor
    /// and clears it when the tab closes — an editor-less document fires no
    /// type-change events.
    void setSink(wxEvtHandler* sink) { m_sink = sink; }

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
    /// mutation is forwarded to `ProjectBase::setNodePath` so the project's
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

    /// Force the modified state. `setModified(true)` flips the meta-dirty bit
    /// (used when the on-disk file is deleted, so the buffer reads as unsaved
    /// without touching the editor's own save-point machinery); `false`
    /// delegates to `markSaved()`.
    void setModified(bool modified);

    /// Check if file was modified externally since last load/save. Compares
    /// both on-disk mod-time and size — size catches a same-second overwrite
    /// the mod-time granularity (2s on FAT / some network shares) would miss.
    [[nodiscard]] auto checkExternalChange() const -> bool;

    /// Update stored modification time + size from the file on disk. Called
    /// after open / save / reload so the next `checkExternalChange` measures
    /// against the current on-disk state (this also suppresses the watcher
    /// reacting to our own writes).
    void updateModTime();

    /// Pending external-change state awaiting user resolution.
    [[nodiscard]] auto getPendingExternal() const -> ExternalChange { return m_pendingExternal; }
    /// Set the pending external-change state.
    void setPendingExternal(const ExternalChange state) { m_pendingExternal = state; }

    /// Show the external-change info bar for `kind` on this document's page.
    /// Routed to the `EditorPanel` view, which owns the bar; no-op view-less.
    void showExternalBar(ExternalChange kind);
    /// Hide the external-change info bar. No-op when the document is view-less.
    void hideExternalBar();

    /// Resolve any pending external-change notification: re-baseline to the
    /// current on-disk state (so it won't immediately re-trigger) and hide the
    /// bar. Called when the user edits or saves — they've implicitly chosen to
    /// keep working on their version. No-op when nothing is pending.
    void dismissExternalNotification();

    /// Update document controls settings. Forwards to the view when
    /// the view is an `EditorPanel`; no-op for other view kinds.
    void updateSettings();

    /// Write this document's persistable attributes into `cfg` at its
    /// already-selected path/group, limited to `scope` (see `SessionScope`):
    /// project-scope writes encoding / EOL (when non-default) and the type
    /// override; runtime-scope writes scroll / caret / folds. Shared by the
    /// `.fbs` session, the `.fbide/session.ini`, and the `.fbp`.
    void setSessionAttributes(wxConfigBase& cfg, SessionScope scope);

    /// Read + apply the `scope` slice of attributes previously written by
    /// `setSessionAttributes` (cfg's path/group already selected by the caller).
    void loadSessionAttributes(const wxConfigBase& cfg, SessionScope scope);

    /// The project that owns this document — the shared `EphemeralProject`
    /// for a standalone file, or a persistent `Project` for a project member.
    /// Never null once the document is owned (which it is for its whole
    /// lifetime); the owning project always outlives it.
    [[nodiscard]] auto getProject() const -> ProjectBase* { return m_project; }

    /// The persistent-project file node that owns this document, or `nullptr`
    /// when it isn't bound to a persistent `Project` (standalone/ephemeral).
    /// Lets session/persistence code key a document by its `Node::Id`.
    [[nodiscard]] auto getNode() const -> Project::Node*;

    /// Bind this document to a project. For a persistent project pass the
    /// owning file `node` (path resolves through it). For the shared
    /// ephemeral pass no node — the document keeps its own path in `m_source`.
    void bindToProject(ProjectBase* project, Project::Node* node = nullptr);

private:
    ConfigManager& m_config;                   ///< Source of encoding/EOL defaults and the locale table.
    Source m_source;                           ///< Path (unbound) or node ID (project-bound); see `Source`.
    ProjectBase* m_project = nullptr;          ///< Owning project; non-null iff `m_source` holds `ProjectBase::Node*`.
    DocumentType m_type;                       ///< Document type — drives lexer + theme dispatch.
    bool m_typeOverridden = false;             ///< True when the user explicitly picked the type.
    Unowned<wxWindow> m_view;                  ///< Generic view back-link — wx-parented to the notebook.
    Unowned<Editor> m_editor;                  ///< Typed editor pointer when the view hosts one; null otherwise.
    std::filesystem::file_time_type m_modTime; ///< Last on-disk mtime — backs `checkExternalChange`.
    std::uintmax_t m_size = 0;                  ///< Last on-disk size — paired with mtime for change detection.
    TextEncoding m_encoding;                   ///< Bytes-to-text codec used on save.
    EolMode m_eolMode;                         ///< Line-ending convention applied on save.
    ExternalChange m_pendingExternal = ExternalChange::None; ///< Unresolved external change awaiting the user.
    /// Set when encoding is changed; cleared on save. OR'd with the
    /// view's modify flag in isModified() so encoding-only edits still
    /// show as dirty.
    bool m_metaModified = false;
    wxEvtHandler* m_sink = nullptr; ///< Sink for `EVT_DOCUMENT_TYPE_CHANGED`; null = no observer.
};

} // namespace fbide
