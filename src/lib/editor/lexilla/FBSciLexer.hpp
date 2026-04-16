//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "DefaultLexer.h"

namespace Lexilla { class StyleContext; }

namespace fbide {

enum class FBSciLexerState : int {
    /// Unknown, default token style
    Default,
    /// Single line comment starting with ' or REM
    Comment,
    /// Multiline nested comment /' ... '/
    MultilineComment,
    /// Any valid number
    Number,
    /// String literal
    String,
    /// Unclosed string literal
    StringOpen,
    /// Any non-keyword identifier
    Identifier,
    /// Keyword groups
    Keyword1,
    Keyword2,
    Keyword3,
    Keyword4,
    Keyword5,
    /// Operator symbols
    Operator,
    /// Label
    Label,
    /// Built-in constant values, such as true, false, etc.
    Constant,
    /// Preprocessor
    Preprocessor,
};

/// Allow simple conversion from enum to int
/// int labelId = +FBSciLexerState::Label;
static constexpr auto operator+(const FBSciLexerState& rhs) -> int {
    return static_cast<int>(rhs);
}

/// Custom Scintilla lexer for FreeBASIC.
class FBSciLexer final : public Lexilla::DefaultLexer {
    FBSciLexer();
public:
    /// Number of keyword groups our lexer supports
    static constexpr int WORD_LIST_COUNT = 5;

    const char* SCI_METHOD DescribeWordListSets() override;
    Sci_Position SCI_METHOD WordListSet(int n, const char* wl) override;

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    /// Factory method for Scintilla.
    static auto Create() -> Scintilla::ILexer5*;

private:
    void lexDefault(Lexilla::StyleContext& sc) const noexcept;
    void lexComment(Lexilla::StyleContext& sc) const noexcept;
    void lexMultilineComment(Lexilla::StyleContext& sc) const noexcept;
    void lexNumber(Lexilla::StyleContext& sc) const noexcept;
    void lexString(Lexilla::StyleContext& sc) const noexcept;
    void lexStringOpen(Lexilla::StyleContext& sc) const noexcept;
    void lexIdentifier(Lexilla::StyleContext& sc) const noexcept;
    void lexPreprocessor(Lexilla::StyleContext& sc) const noexcept;

    std::array<Lexilla::WordList, WORD_LIST_COUNT> m_wordLists;
};

} // namespace fbide
