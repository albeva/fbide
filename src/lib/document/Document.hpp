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

class wxStyledTextCtrlMiniMap;

namespace fbide {
class Context;
class Editor;
class DocumentInfoBar;

/**
 * One open file (or untitled buffer): editor widget plus the metadata
 * that lets us round-trip to disk — path, type, encoding, EOL,
 * mod-time, latest symbol table.
 *
 * **Owns:** the wx-parented container panel (`Unowned<wxPanel>`) that
 * holds the `Editor` widget and the experimental minimap, plus the
 * `shared_ptr<const SymbolTable>` published by intellisense.
 * **Owned by:** `DocumentManager` via `unique_ptr<Document>`.
 * **Lifetime:** matches the notebook tab.
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

    /// Pending external-change state, surfaced via the page's info bar and
    /// a tab marker until the user resolves it.
    enum class ExternalChange : std::uint8_t {
        None,     ///< In sync with disk.
        Conflict, ///< Changed on disk while the buffer has unsaved edits.
        Deleted,  ///< Removed from disk while still open.
    };

    /// Create a new document. Editor is created as child of parent.
    Document(wxWindow* parent, Context& ctx, DocumentType type = DocumentType::FreeBASIC);

    /// Get the file path. Empty if untitled. Returned as `std::filesystem::path`
    /// — callers that hand it to a wx API should wrap with `toWxString(...)`.
    [[nodiscard]] auto getFilePath() const -> const std::filesystem::path& { return m_filePath; }

    /// Set the file path.
    void setFilePath(const std::filesystem::path& path);

    /// Get display title for tab (filename or "Untitled").
    [[nodiscard]] auto getTitle() const -> wxString;

    /// Get document type.
    [[nodiscard]] auto getType() const -> DocumentType { return m_type; }

    /// Override the document type (user picked from the status bar menu).
    /// Sticky across Save / Save As until the document is closed — the
    /// override flag is per-instance, only persisted via FileSession.
    /// Reapplies editor settings (lexer + theme).
    void setType(DocumentType type);

    /// True when the type was set explicitly via `setType` rather than
    /// derived from the file path.
    [[nodiscard]] auto isTypeOverridden() const -> bool { return m_typeOverridden; }

    /// Get the editor widget.
    [[nodiscard]] auto getEditor() -> Editor* { return m_editor; }
    /// Const overload of `getEditor`.
    [[nodiscard]] auto getEditor() const -> const Editor* { return m_editor; }

    /// Get the notebook page — the container panel holding editor + minimap.
    [[nodiscard]] auto getPage() -> wxWindow* { return m_container; }
    /// Const overload of `getPage`.
    [[nodiscard]] auto getPage() const -> const wxWindow* { return m_container; }

    /// Enable or disable the minimap. Effective visibility also depends
    /// on the page being wide enough — see `updateMinimapVisibility`.
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

    /// Is the document modified?
    [[nodiscard]] auto isModified() const -> bool;

    /// Set modified state. Also clears the encoding-change dirty flag
    /// when called with `false` (e.g. after successful save).
    void setModified(bool modified);

    /// Check if file was modified externally since last load/save. Compares
    /// both on-disk mod-time and size — size catches same-second edits the
    /// mod-time granularity (2s on FAT / some network shares) would miss.
    [[nodiscard]] auto checkExternalChange() const -> bool;

    /// Update stored modification time + size from the file on disk. Called
    /// after open / save / reload so the next `checkExternalChange` measures
    /// against the current on-disk state (this is also what suppresses the
    /// watcher reacting to our own writes).
    void updateModTime();

    /// Pending external-change state awaiting user resolution.
    [[nodiscard]] auto getPendingExternal() const -> ExternalChange { return m_pendingExternal; }
    /// Set the pending external-change state (info bar + tab marker).
    void setPendingExternal(const ExternalChange state) { m_pendingExternal = state; }

    /// Show the external-change info bar for `kind` on this page.
    void showExternalBar(ExternalChange kind);
    /// Hide the external-change info bar.
    void hideExternalBar();

    /// Show a save-failure message in this page's notification bar.
    void showSaveError(const wxString& message);
    /// Clear the save-failure bar (e.g. after a later successful save). No-op
    /// unless an error is currently shown.
    void dismissSaveError();

    /// Resolve any pending external-change notification: re-baseline to the
    /// current on-disk state (so it won't immediately re-trigger) and hide the
    /// bar. Called when the user edits or saves — they've implicitly chosen to
    /// keep working on their version. No-op when nothing is pending.
    void dismissExternalNotification();

    /// Latest symbol table produced by IntellisenseService for this document.
    /// May be null until the first parse completes.
    [[nodiscard]] auto getSymbolTable() const
        -> std::shared_ptr<const SymbolTable> { return m_symbolTable; }

    /// Set the latest symbol table. Called by DocumentManager from the
    /// IntellisenseService result handler on the UI thread.
    void setSymbolTable(std::shared_ptr<const SymbolTable> table) {
        m_symbolTable = std::move(table);
    }

    /// Update document controls settings
    void updateSettings();

    /// Compiler configuration this document is pinned to (slug, e.g.
    /// `"cfg-1"`). Empty means "follow whatever is currently active" —
    /// see `docs/compiler-configurations.md`.
    [[nodiscard]] auto getConfiguration() const noexcept -> const std::optional<wxString>& { return m_configuration; }

    /// Pin the document to a specific configuration, or clear the pin.
    /// CompilerManager owns the normalisation rule ("matches active →
    /// empty"); this setter is a dumb assignment. Does NOT mark the
    /// document modified — configuration lives in the session file, not
    /// in the document contents.
    void setConfiguration(std::optional<wxString> slug) noexcept { m_configuration = std::move(slug); }

private:
    /// Page resized — re-evaluate whether the minimap still fits.
    void onContainerSize(wxSizeEvent& event);
    /// Show/hide the minimap based on the current page width.
    void updateMinimapVisibility() const;
    /// Create the minimap widget and dock it into the page layout.
    void createMinimap();
    /// Destroy the minimap widget and drop it from the page layout.
    void destroyMinimap();

    Context& m_ctx;                             ///< Application context.
    wxString m_compiledFile;                    ///< Path of the most recently compiled executable.
    std::filesystem::path m_filePath;           ///< Absolute path on disk; empty for new documents.
    DocumentType m_type;                        ///< Document type — drives lexer + theme dispatch.
    bool m_typeOverridden = false;              ///< True when the user explicitly picked the type.
    Unowned<wxPanel> m_container;               ///< wx-parented notebook page — holds info bar + editor + minimap.
    Unowned<DocumentInfoBar> m_infoBar;         ///< External-change notification bar, docked at the page top.
    Unowned<Editor> m_editor;                   ///< Editor widget, child of m_container.
    Unowned<wxStyledTextCtrlMiniMap> m_minimap; ///< Minimap — lazily created; null while disabled.
    wxBoxSizer* m_editorSizer = nullptr;        ///< Inner horizontal sizer holding editor + minimap (below the info bar).
    int m_minimapWidth;                         ///< Minimap width in px — `editor.minimapWidth` config key.
    bool m_minimapEnabled;                      ///< Minimap toggle state — `commands.viewMinimap`.
    std::filesystem::file_time_type m_modTime;  ///< Last on-disk mtime — backs `checkExternalChange`.
    std::uintmax_t m_size = 0;                   ///< Last on-disk size — paired with mtime for change detection.
    ExternalChange m_pendingExternal = ExternalChange::None; ///< Unresolved external change awaiting the user.
    TextEncoding m_encoding;                    ///< Bytes-to-text codec used on save.
    EolMode m_eolMode;                          ///< Line-ending convention applied on save.
    /// Set when encoding is changed; cleared on save. OR'd with editor's
    /// modify flag in isModified() so encoding-only edits still show as dirty.
    bool m_metaModified = false;
    std::shared_ptr<const SymbolTable> m_symbolTable; ///< Latest intellisense result for this document.
    std::optional<wxString> m_configuration;          ///< Pinned compiler config slug; empty = follow active.
};

} // namespace fbide
