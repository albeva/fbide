//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Panel.hpp"

namespace fbide {

/// Compiler settings tab — paths and command prototypes.
class CompilerPage final : public Panel {
public:
    explicit CompilerPage(Context& ctx, wxWindow* parent);
    void layout() override;
    void apply() override;

private:
    void onCompilerBrowse(wxCommandEvent& event);
    void onHelpBrowse(wxCommandEvent& event);

    Unowned<wxTextCtrl> m_textCompilerPath;
    Unowned<wxTextCtrl> m_textCompileCommand;
    Unowned<wxTextCtrl> m_textRunCommand;
    Unowned<wxTextCtrl> m_textHelpFile;
};

} // namespace fbide
