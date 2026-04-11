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

/// Scintilla-based code editor.
class Editor final : public wxStyledTextCtrl {
public:
    /// Create editor as child of parent window for given document type.
    Editor(wxWindow* parent, Context& ctx, DocumentType type = DocumentType::FreeBASIC);

    /// Apply theme and settings from context.
    void applySettings();

    /// Get document type.
    [[nodiscard]] auto getDocType() const -> DocumentType { return m_docType; }

    /// Change document type
    void setDocType(DocumentType type);

    /// Select current line
    void selectLine();

private:
    void applyEditorSettings();
    void applyTheme();
    void applyFreebasicTheme();
    void applyHtmlTheme();
    void applyTextTheme();

    Context& m_ctx;
    DocumentType m_docType;
};

} // namespace fbide
