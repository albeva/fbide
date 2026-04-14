//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class BBCodeText;
class Context;

/// Dialog for displaying the compiler log with [bold] markup support.
class CompilerLog final : public wxDialog {
public:
    NO_COPY_AND_MOVE(CompilerLog)

    CompilerLog(wxWindow* parent, const wxString& title);

    /// Set up the layout. Call after construction.
    void create(const Context& ctx);

    /// Clear all log content.
    void clear();

    /// Append a line to the log. Supports [bold]...[/bold] markup.
    void log(const wxString& line);

    /// Append multiple lines to the log.
    void log(const wxArrayString& lines);

private:
    Unowned<BBCodeText> m_output;
};

} // namespace fbide
