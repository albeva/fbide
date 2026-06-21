//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::markdown {

/// One syntax-coloured run within a code line. `text` never spans lines.
struct CodeRun {
    wxString text;               ///< Run text — never contains a newline.
    wxColour colour;             ///< Foreground colour.
    bool bold       : 1 = false; ///< Bold typeface.
    bool italic     : 1 = false; ///< Italic typeface.
    bool underlined : 1 = false; ///< Underlined.

    auto operator==(const CodeRun&) const -> bool = default;
};

/// A code line — a sequence of coloured runs. Empty for a blank line.
using CodeLine = std::vector<CodeRun>;

/// True for the fence language tags that denote FreeBASIC source.
[[nodiscard]] auto isFreeBasicTag(const wxString& lang) -> bool;

/// Split `code` into one plain-coloured `CodeLine` per source line, using the
/// system window-text colour and no styling. This is the fallback used when no
/// syntax highlighter is injected, and for non-FreeBASIC fences. A single
/// trailing blank line (md4c terminates a fenced block with '\n') is dropped.
[[nodiscard]] auto plainCodeLines(const wxString& code) -> std::vector<CodeLine>;

} // namespace fbide::markdown
