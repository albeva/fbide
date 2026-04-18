//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/Panel.hpp"

namespace fbide {

/// Compiler settings tab — paths and command prototypes.
class CompilerPage final : public Panel {
public:
    NO_COPY_AND_MOVE(CompilerPage)

    explicit CompilerPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

private:
    void compilerPath();
    void compilerCommand();
    void runCommand();
#ifdef __WXMSW__
    void helpFile();
#endif

    auto makeEntryField(wxString& value, LangId lang) -> Unowned<wxTextCtrl>;
    auto makeFileEntry(wxString& value, LangId lang) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>>;

    wxString m_compilerPath;
    wxString m_compileCommand;
    wxString m_runCommand;
#ifdef __WXMSW__
    wxString m_helpFile;
#endif
};

} // namespace fbide
