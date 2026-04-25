//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {

/// Compiler settings tab — paths and command prototypes.
class CompilerPage final : public Panel {
public:
    NO_COPY_AND_MOVE(CompilerPage)

    explicit CompilerPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

private:
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    void compilerPath();
    void compilerCommand();
    void runCommand();
#ifdef __WXMSW__
    void helpFile();
#endif

    auto makeEntryField(wxString& value, const wxString& labelText) -> Unowned<wxTextCtrl>;
    auto makeFileEntry(wxString& value, const wxString& labelText) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>>;

    wxString m_compilerPath;
    wxString m_compileCommand;
    wxString m_runCommand;
#ifdef __WXMSW__
    wxString m_helpFile;
#endif
};

} // namespace fbide
