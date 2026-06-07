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

/// Logical sections of the compiler log, in display order. The run
/// sections follow SystemInfo because `appendSystemInfo` runs at the end
/// of the compile step, before the run step begins.
enum class CompilerLogSection : std::size_t {
    CompilerCommand, ///< The fbc command line.
    CompilerOutput,  ///< Raw compiler diagnostics.
    CompileResult,   ///< Success / failure summary and generated executable.
    SystemInfo,      ///< FBIde / fbc / OS versions.
    RunCommand,      ///< The run command line.
    RunResult,       ///< Run exit code or launch failure.
};

/// Dialog that displays the compiler log: bold section titles over
/// verbatim section content in a read-only fixed-width text control.
class CompilerLog final : public wxDialog {
public:
    NO_COPY_AND_MOVE(CompilerLog)

    /// Construct without populating widgets; `create()` builds them.
    CompilerLog(wxWindow* parent, const wxString& title);

    /// Set up the layout. Call after construction.
    void create(Context& ctx);

    /// Set a section's literal content (no markup).
    void add(CompilerLogSection section, const wxString& text);

    /// Clear every section.
    void clear();

    /// hande window sohw event
    void onShow(wxShowEvent& event);

private:
    void render();
    void render(CompilerLogSection section, const wxString& text) const;

    using Container = std::vector<std::pair<CompilerLogSection, wxString>>;

    Context* m_ctx = nullptr;     ///< For localized section titles (tr is non-const).
    Unowned<wxTextCtrl> m_output; ///< Read-only fixed-width log view.
    wxTextAttr m_normal;          ///< Section content style.
    wxTextAttr m_bold;            ///< Section title style.
    Container m_sections;         ///< Per-section content, rendered by render().

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
