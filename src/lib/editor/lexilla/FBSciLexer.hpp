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
    /// Default-constructed lexer. Keywords come from the shared table set via
    /// `setKeywords` — there are no per-instance wordlists.
    FBSciLexer();
    /// Set property
    Sci_Position SCI_METHOD PropertySet(const char* key, const char* val) override;

    /// (Re)build the shared keyword tables from per-group keyword strings
    /// (space-separated, lowercase; group order matches kThemeKeywordCategories).
    /// Call once at startup and whenever the user's keyword settings change.
    /// The new table is published atomically, so this is safe to call from the
    /// UI thread while a worker-thread lexer runs — in-flight `Lex` calls keep
    /// using the snapshot they pinned.
    static void setKeywords(const std::array<std::string, kThemeKeywordGroupsCount>& groups);

    /// Scintilla hook — colorize the document range starting at `startPos`.
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    /// Scintilla hook — compute fold levels over the document range.
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    /// Factory method for Scintilla.
    static auto Create() -> Scintilla::ILexer5*;

    /// Per-line asm parsing state. Drives keyword-wordlist selection and
    /// the single-line-vs-block-opener resolution at logical EOL.
    enum class AsmState : std::uint8_t {
        None = 0,         ///< Not inside any asm context.
        Undetermined = 1, ///< Saw `asm` keyword; no significant content yet — block-vs-stmt undecided.
        Block = 2,        ///< Confirmed multi-line `asm … end asm` block.
        Stmt = 3,         ///< Confirmed single-line `asm <stmt>` statement.
    };

    /// Per-line state stored via IDocument::SetLineState / GetLineState.
    /// Packed into a single int for Scintilla compatibility.
    /// Public so the analyses/lexer adapter can read it via IStyledSource.
    struct alignas(int) LineState final {
        constexpr LineState() = default;
        bool continueLine    : 1 = false;          ///< Line ends in `_` continuation.
        bool isFirst         : 1 = false;          ///< This is the first significant line of the source.
        bool continuePP      : 1 = false;          ///< Inside a continued preprocessor directive.
        bool fieldAccess     : 1 = false;          ///< Last token was `.` or `->` — next ident is a field.
        bool ppDirectiveSeen : 1 = false;          ///< In PP body, the directive identifier (first ident after `#`) was already classified.
        AsmState asmState    : 3 = AsmState::None; ///< Asm context tracker (None/Undetermined/Block/Stmt).

        std::uint8_t commentNestLevel = 0; ///< Open `/'` block-comment nesting level.
        std::uint8_t reserved1 = 0;        ///< Reserved for future use.
        std::uint8_t reserved2 = 0;        ///< Reserved for future use.

        /// Convert from Scintilla line state int
        static constexpr auto fromInt(const int value) noexcept -> LineState {
            return std::bit_cast<LineState>(value);
        }

        /// Convert to Scintilla line state int
        [[nodiscard]] constexpr auto toInt() const noexcept -> int {
            return std::bit_cast<int>(*this);
        }
    };
    static_assert(sizeof(LineState) == sizeof(int) && alignof(LineState) == alignof(int));

    /// Options
    struct Options final {
        bool fold = false;
    };

private:
    /// Form of the number being lexed
    enum class NumberForm : std::uint8_t {
        Decimal,     ///< Plain decimal integer / float without prefix.
        Fraction,    ///< Decimal point seen — accumulating fractional digits.
        Exponent,    ///< `e` / `E` seen — accumulating exponent digits.
        Hexadecimal, ///< `&H` prefix.
        Octal,       ///< `&O` prefix.
        Binary       ///< `&B` prefix.
    };

    /// Per-line setup at the start of styling.
    FBIDE_INLINE void lexLineStart() noexcept;
    /// Persist line state at end-of-line.
    FBIDE_INLINE void lexLineEnd() noexcept;
    /// Drop back to the Default state machine for the next char.
    FBIDE_INLINE void resetToDefault() noexcept;
    /// True when the current position can resolve as a member access target.
    FBIDE_INLINE bool canAccessMember() const noexcept;
    /// State: Default — dispatch to the right lexer for the next char.
    FBIDE_INLINE void lexDefault() noexcept;
    /// State: single-line comment (`'`).
    FBIDE_INLINE void lexComment()const noexcept;
    /// State: nested block comment (`/'`).
    FBIDE_INLINE void lexMultilineComment() noexcept;
    /// State: numeric literal.
    FBIDE_INLINE void lexNumber() noexcept;
    /// State: string literal (open quote seen).
    FBIDE_INLINE void lexStringOpen() noexcept;
    /// State: identifier — accumulate, then dispatch to keyword classification.
    FBIDE_INLINE void lexIdentifier() noexcept;
    /// Try to classify the current identifier as a keyword. Returns true on hit.
    FBIDE_INLINE auto identifyKeyword() noexcept -> bool;
    /// State: operator / punctuation.
    FBIDE_INLINE void lexOperator() noexcept;
    /// State: preprocessor directive line (entire line styled as PP).
    FBIDE_INLINE void lexPreprocessor() noexcept;

    /// Sentinel "no current line" value.
    static constexpr Sci_Position INVALID_LINE = std::numeric_limits<Sci_Position>::max() - 1;
    /// Identifier-buffer capacity.
    static constexpr std::size_t MAX_IDENT_LEN = 128;

    Lexilla::StyleContext* m_sc = nullptr;                               ///< Active Scintilla styling context (per Lex call).
    Lexilla::LexAccessor* m_styler = nullptr;                            ///< Accessor for line/state queries (per Lex call).
    Sci_Position m_line = 0;                                             ///< Current source line being styled.
    LineState m_previousLineState;                                       ///< State at the start of the current line.
    LineState m_lineState;                                               ///< Live state for the current line.
    NumberForm m_numberForm = NumberForm::Decimal;                       ///< Current numeric literal sub-state.
    bool m_isFirst = true;                                               ///< True until we see the first significant char on the line.
    bool m_fieldAccess = false;                                          ///< Set after `.`/`->` — suppress keyword classification next.
    bool m_slashEscapableString = false;                                 ///< Inside a string with `$` escape sequences enabled.
    bool m_inPpBody = false;                                             ///< Inside a `#`-directive line — drives *PP style retro-painting and Preprocessor return state.
    bool m_ppDirectiveSeen = false;                                      ///< In PP body, the directive identifier (first ident after `#`) was already classified — survives `_` continuation + nested block comments.
    AsmState m_asmState = AsmState::None;                                ///< Active asm context (drives wordlist selection and EOL resolution).
    std::array<char, MAX_IDENT_LEN> m_identBuffer {};                    ///< Reusable identifier-spelling buffer.
    Options m_options {};                                                ///< lexilla options
};

} // namespace fbide
