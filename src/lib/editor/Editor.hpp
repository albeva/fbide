//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Scintilla-based code editor.
class Editor final : public wxStyledTextCtrl {
public:
    /// Create editor as child of parent window.
    Editor(wxWindow* parent, Context& ctx);

    /// Apply theme and settings from context.
    void applySettings();

private:
    void applyTheme();
    void applyEditorSettings();

    Context& m_ctx;
};

} // namespace fbide
