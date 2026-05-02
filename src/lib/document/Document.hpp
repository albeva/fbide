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
class Editor;

/**
 * One open file (or untitled buffer): editor widget plus the metadata
 * that lets us round-trip to disk — path, type, encoding, EOL,
 * mod-time, latest symbol table.
 *
 * **Owns:** the wx-parented `Editor` widget (`Unowned<Editor>`) plus
 * the `shared_ptr<const SymbolTable>` published by intellisense.
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

    /// Create a new document. Editor is created as child of parent.
    Document(wxWindow* parent, Context& ctx, DocumentType type = DocumentType::FreeBASIC);

    /// Get the file path. Empty if untitled.
    [[nodiscard]] auto getFilePath() const -> const wxString& { return m_filePath; }

    /// Set the file path.
    void setFilePath(const wxString& path);

    /// Get display title for tab (filename or "Untitled").
    [[nodiscard]] auto getTitle() const -> wxString;

    /// Get document type.
    [[nodiscard]] auto getType() const -> DocumentType { return m_type; }

    /// Get the editor widget.
    [[nodiscard]] auto getEditor() -> Editor* { return m_editor; }
    /// Const overload of `getEditor`.
    [[nodiscard]] auto getEditor() const -> const Editor* { return m_editor; }

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

private:
    Context& m_ctx;                                ///< Application context.
    wxString m_compiledFile;                       ///< Path of the most recently compiled executable.
    wxString m_filePath;                           ///< Absolute path on disk; empty for new documents.
    DocumentType m_type;                           ///< Document type — drives lexer + theme dispatch.
    Unowned<Editor> m_editor;                      ///< wx-parented editor widget.
    wxDateTime m_modTime;                          ///< Last on-disk mtime — backs `checkExternalChange`.
    TextEncoding m_encoding;                       ///< Bytes-to-text codec used on save.
    EolMode m_eolMode;                             ///< Line-ending convention applied on save.
    /// Set when encoding is changed; cleared on save. OR'd with editor's
    /// modify flag in isModified() so encoding-only edits still show as dirty.
    bool m_metaModified = false;
    std::shared_ptr<const SymbolTable> m_symbolTable; ///< Latest intellisense result for this document.
};

} // namespace fbide
