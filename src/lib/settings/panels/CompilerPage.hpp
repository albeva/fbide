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
    wxString m_compilerPath;
    wxString m_compileCommand;
    wxString m_runCommand;
    wxString m_helpFile;
};

} // namespace fbide
