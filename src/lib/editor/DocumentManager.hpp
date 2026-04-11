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

namespace fbide {
class Context;

/// Manages open documents and their notebook tabs.
class DocumentManager final {
public:
    explicit DocumentManager(Context& ctx);

    /// Create a new empty document and add it as a tab.
    auto createNew(DocumentType type = DocumentType::FreeBASIC) -> Document&;

    /// Open a file. Returns existing document if already open, or nullptr on failure.
    auto open(const wxString& filePath) -> Document*;

    /// Close a document. Returns false if user cancelled (unsaved changes).
    auto close(Document& doc) -> bool;

    /// Close all documents. Returns false if user cancelled.
    auto closeAll() -> bool;

    /// Get currently active document (selected tab), or nullptr if none.
    [[nodiscard]] auto getActive() const -> Document*;

    /// Get document count.
    [[nodiscard]] auto getCount() const -> size_t { return m_documents.size(); }

    /// Find document by file path. Returns nullptr if not found.
    [[nodiscard]] auto findByPath(const wxString& path) const -> Document*;

    /// Get number of modified documents.
    [[nodiscard]] auto getModifiedCount() const -> size_t;

private:
    /// Find document by its editor widget.
    [[nodiscard]] auto findByEditor(const wxWindow* editor) const -> Document*;

    /// Find notebook page index for a document.
    [[nodiscard]] auto findPageIndex(const Document& doc) const -> int;

    /// Get the notebook from UIManager.
    [[nodiscard]] auto getNotebook() const -> wxAuiNotebook*;

    Context& m_ctx;
    std::vector<std::unique_ptr<Document>> m_documents;
};

} // namespace fbide
