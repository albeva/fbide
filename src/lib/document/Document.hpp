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
#include "format/transformers/reformat/FormatTree.hpp"

namespace fbide {
class Context;
class Editor;

/// Represents a single open document with its editor widget.
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
    [[nodiscard]] auto getEditor() const -> const Editor* { return m_editor; }

    /// Get compiled file path
    [[nodiscard]] auto getCompiledFile() const -> wxString { return m_compiledFile; }

    /// Set compiled file path
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

    /// Latest parse tree produced by IntellisenseService for this document.
    /// May be null until the first parse completes.
    [[nodiscard]] auto getProgramTree() const
        -> std::shared_ptr<const reformat::ProgramTree> { return m_programTree; }

    /// Set the latest parse tree. Called by DocumentManager from the
    /// IntellisenseService result handler on the UI thread.
    void setProgramTree(std::shared_ptr<const reformat::ProgramTree> tree) {
        m_programTree = std::move(tree);
    }

private:
    Context& m_ctx;
    wxString m_compiledFile;
    wxString m_filePath;
    DocumentType m_type;
    Unowned<Editor> m_editor;
    wxDateTime m_modTime;
    TextEncoding m_encoding;
    EolMode m_eolMode;
    /// Set when encoding is changed; cleared on save. OR'd with editor's
    /// modify flag in isModified() so encoding-only edits still show as dirty.
    bool m_metaModified = false;
    std::shared_ptr<const reformat::ProgramTree> m_programTree;
};

} // namespace fbide
