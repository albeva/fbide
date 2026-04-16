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
    /// Per-line state stored via IDocument::SetLineState / GetLineState.
    /// Packed into a single int for Scintilla compatibility.
    struct alignas(int) LineState final {
        bool continueLine:1 = false;
        bool isFirst:1 = false;
        bool continuePP: 1 = false;
        bool fieldAccess: 1 = false;

        std::uint8_t commentNestLevel = 0;
        std::uint8_t reserved1 = 0;
        std::uint8_t reserved2 = 0;

        /// Convert from Scintilla line state int
        static constexpr auto fromInt(const int value) noexcept -> LineState {
            return std::bit_cast<LineState>(value);
        }

        /// Convert to Scintilla line state int
        constexpr auto toInt() const noexcept -> int {
            return std::bit_cast<int>(*this);
        }
    };
    static_assert(sizeof(LineState) == sizeof(int) && alignof(LineState) == alignof(int));

    FBIDE_INLINE void lexLineStart() noexcept;
    FBIDE_INLINE void lexLineEnd() noexcept;
    FBIDE_INLINE void resetToDefault() noexcept;
    FBIDE_INLINE bool canAccessMember() noexcept;
    FBIDE_INLINE void lexDefault() noexcept;
    FBIDE_INLINE void lexComment() noexcept;
    FBIDE_INLINE void lexMultilineComment() noexcept;
    FBIDE_INLINE void lexNumber() noexcept;
    FBIDE_INLINE void lexStringOpen() noexcept;
    FBIDE_INLINE void lexIdentifier() noexcept;
    FBIDE_INLINE void identifyKeyword() noexcept;
    FBIDE_INLINE void lexOperator() noexcept;
    FBIDE_INLINE void lexPreprocessor() noexcept;

    /// Invalid line marker
    static constexpr Sci_Position INVALID_LINE = std::numeric_limits<Sci_Position>::max() - 1;
    static constexpr std::size_t MAX_IDENT_LEN = 128;

    Lexilla::StyleContext* m_sc = nullptr;
    Lexilla::LexAccessor* m_styler = nullptr;
    std::array<Lexilla::WordList, WORD_LIST_COUNT> m_wordLists;
    bool m_isFirst = true;
    bool m_fieldAccess = false;
    Sci_Position m_line = 0;
    LineState m_previousLineState;
    LineState m_lineState;
    std::array<char, MAX_IDENT_LEN> m_identBuffer{};
};

} // namespace fbide
