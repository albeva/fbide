//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "DocumentType.hpp"

namespace fbide {
class Context;
class Editor;

/// Represents a single open document with its editor widget.
class Document final {
public:
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

    /// Is this an untitled (never saved) document?
    [[nodiscard]] auto isUntitled() const -> bool { return m_filePath.empty(); }

    /// Is the document modified?
    [[nodiscard]] auto isModified() const -> bool;

    /// Set modified state.
    void setModified(bool modified) const;

    /// Check if file was modified externally since last load/save.
    [[nodiscard]] auto checkExternalChange() const -> bool;

    /// Update stored modification time from file on disk.
    void updateModTime();

private:
    wxString m_compiledFile;
    wxString m_filePath;
    DocumentType m_type;
    Unowned<Editor> m_editor;
    wxDateTime m_modTime;
};

} // namespace fbide
