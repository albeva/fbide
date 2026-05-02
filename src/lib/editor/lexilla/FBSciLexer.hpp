//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
// Order is important!
// clang-format off
#include "ILexer.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "DefaultLexer.h"
// clang-format on
#include "config/ThemeCategory.hpp"

namespace Lexilla {
class StyleContext;
}

namespace fbide {

/**
 * Custom Scintilla lexer for FreeBASIC. Produces style runs over a
 * `Scintilla::IDocument` (live editor or `MemoryDocument`) and folds
 * blocks for the editor's fold margin.
 *
 * Per-line state (`LineState`) is packed into a single `int` and
 * round-tripped through `IDocument::SetLineState` /
 * `GetLineState` — Scintilla persists it per line, the lexer reads
 * it on resume. `LineState` is exposed publicly so the analyses
 * `IStyledSource` adapter can read it.
 *
 * The same lexer instance is reused across `Editor` (UI thread) and
 * `IntellisenseService::m_lexer` (worker thread) — but never
 * concurrently: each owner has its own instance.
 *
 * See @ref editor and @ref analyses.
 */
class FBSciLexer final : public Lexilla::DefaultLexer {
public:
    FBSciLexer();
    const char* SCI_METHOD DescribeWordListSets() override;
    Sci_Position SCI_METHOD WordListSet(int n, const char* wl) override;

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    /// Factory method for Scintilla.
    static auto Create() -> Scintilla::ILexer5*;

    /// Per-line state stored via IDocument::SetLineState / GetLineState.
    /// Packed into a single int for Scintilla compatibility.
    /// Public so the analyses/lexer adapter can read it via IStyledSource.
    struct alignas(int) LineState final {
        bool continueLine : 1 = false;
        bool isFirst      : 1 = false;
        bool continuePP   : 1 = false;
        bool fieldAccess  : 1 = false;
        bool asmBlock     : 1 = false;

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

private:
    /// Form of the number being lexed
    enum class NumberForm : std::uint8_t {
        Decimal,
        Fraction,
        Exponent,
        Hexadecimal,
        Octal,
        Binary
    };

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
    FBIDE_INLINE auto identifyKeyword() noexcept -> bool;
    FBIDE_INLINE void lexOperator() noexcept;
    FBIDE_INLINE void lexPreprocessor() noexcept;

    /// Invalid line marker
    static constexpr Sci_Position INVALID_LINE = std::numeric_limits<Sci_Position>::max() - 1;
    static constexpr std::size_t MAX_IDENT_LEN = 128;

    std::array<Lexilla::WordList, kThemeKeywordGroupsCount> m_wordLists;
    Lexilla::StyleContext* m_sc = nullptr;
    Lexilla::LexAccessor* m_styler = nullptr;
    Sci_Position m_line = 0;
    LineState m_previousLineState;
    LineState m_lineState;
    NumberForm m_numberForm = NumberForm::Decimal;
    bool m_isFirst = true;
    bool m_fieldAccess = false;
    bool m_slashEscapableString = false;
    bool m_asmBlock = false;
    std::array<char, MAX_IDENT_LEN> m_identBuffer {};
};

} // namespace fbide
