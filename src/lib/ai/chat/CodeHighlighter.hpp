//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"

namespace Scintilla {
class ILexer5;
}

namespace fbide {
class Theme;
class Value;

/// One syntax-coloured run within a code line. `text` never spans lines.
struct CodeRun {
    wxString text;           ///< Run text — never contains a newline.
    wxColour colour;         ///< Foreground colour.
    bool bold = false;       ///< Bold typeface.
    bool italic = false;     ///< Italic typeface.
    bool underlined = false; ///< Underlined.

    auto operator==(const CodeRun&) const -> bool = default;
};

/// A code line — a sequence of coloured runs. Empty for a blank line.
using CodeLine = std::vector<CodeRun>;

/// Map already-lexed FreeBASIC `tokens` to theme-coloured runs, split into
/// lines on newline boundaries. Pure — no lexer, no I/O — so it is unit
/// testable with hand-built tokens. A single trailing blank line (md4c
/// terminates a fenced code block with '\n') is dropped.
[[nodiscard]] auto highlightCode(const std::vector<lexer::Token>& tokens, const Theme& theme)
    -> std::vector<CodeLine>;

/// FreeBASIC syntax highlighter for chat code blocks. Owns one configured
/// FBSciLexer instance, reused across calls — the same colouring path as the
/// editor, but rendered to coloured runs rather than to styled Scintilla text.
class CodeHighlighter final {
public:
    NO_COPY_AND_MOVE(CodeHighlighter)

    /// Build and configure the lexer from the `groups` keyword config — the
    /// `Value` map normally found at `keywords().at("groups")`.
    explicit CodeHighlighter(const Value& keywordGroups);
    ~CodeHighlighter();

    /// Lex `code` as FreeBASIC and map it to `theme`-coloured lines.
    [[nodiscard]] auto highlight(const wxString& code, const Theme& theme) const
        -> std::vector<CodeLine>;

private:
    Scintilla::ILexer5* m_lexer; ///< Reused FreeBASIC lexer; Release()d in the dtor.
};

} // namespace fbide
