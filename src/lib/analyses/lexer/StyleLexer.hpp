//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <optional>
#include <string>
#include "StyledSource.hpp"
#include "Token.hpp"
#include "config/ThemeCategory.hpp"

namespace fbide {
class Value;
}

namespace fbide::lexer {

/// (Re)build FBSciLexer's shared keyword tables from `kw` (a config `Value`
/// map keyed by `ThemeCategory` name). Keywords are lowercased so FBSciLexer's
/// case-insensitive lookup matches. Call once at startup and whenever the
/// user's keyword settings change — every FBSciLexer instance then classifies
/// against the same tables, with no per-instance wordlists.
void setFbKeywords(const Value& kw);

/// One coalesced run of bytes carrying the same FBSciLexer style.
struct StyleRange {
    ThemeCategory style; ///< Style category for the run.
    Sci_PositionU start; ///< Inclusive start byte offset.
    Sci_PositionU end;   ///< Exclusive end byte offset.
};

/// Token producer that walks FBSciLexer-emitted style runs over an
/// `IStyledSource`. Replaces the standalone char-by-char `Lexer` for any
/// caller that has access to a styled document (the editor or a headless
/// `MemoryDocument` driven through `FBSciLexer`).
class StyleLexer final {
public:
    NO_COPY_AND_MOVE(StyleLexer)

    /// Inclusive-start, exclusive-end byte range. Empty range means whole document.
    using Range = std::pair<Sci_PositionU, Sci_PositionU>;

    /// Construct over a styled source view.
    explicit StyleLexer(IStyledSource& src);
    /// Trivial destructor.
    ~StyleLexer() = default;

    /// Tokenise `range` and return the result. Allocates a fresh vector.
    [[nodiscard]] auto tokenise(const Range& range = {}) -> std::vector<Token> {
        std::vector<Token> tokens;
        tokenise(tokens, range);
        return tokens;
    }

    /// Tokenise `range` into `tokens`, clearing it first. Reuses capacity.
    void tokenise(std::vector<Token>& tokens, const Range& range = {});

private:
    /// Pull the next coalesced style run from the source.
    [[nodiscard]] auto nextStyle() -> std::optional<StyleRange>;
    /// Copy a byte range out of the source as a UTF-8 std::string.
    [[nodiscard]] auto stringFromRange(Sci_PositionU start, Sci_PositionU end) const -> std::string;

    /// Dispatch a style run to the appropriate emit helper.
    void emitFromRange(const StyleRange& range, std::vector<Token>& out);
    /// Emit whitespace / newline tokens from a `Default` style run.
    void emitDefault(const StyleRange& range, std::vector<Token>& out);
    /// Emit operator tokens from an `Operator` / `OperatorPP` style run.
    void emitOperator(const StyleRange& range, std::vector<Token>& out);
    /// Emit an `Identifier` token (with `style` carrying the source style —
    /// `Identifier` for code, `IdentifierPP` for preprocessor body words).
    void emitIdentifier(const StyleRange& range, std::vector<Token>& out);
    /// Emit a keyword token of the given kind.
    void emitKeyword(const StyleRange& range, TokenKind kind, std::vector<Token>& out);
    /// Emit the leading `#` of a preprocessor directive (Preprocessor style
    /// run). Whitespace runs inside a PP body emit as Whitespace tokens.
    void emitPreprocessor(const StyleRange& range, std::vector<Token>& out);
    /// Emit a `KeywordPP`-styled run. The first KeywordPP after the bare
    /// `#` directive token merges into it (setting the directive's
    /// `keywordKind`); subsequent runs emit standalone Preprocessor tokens.
    void emitKeywordPP(const StyleRange& range, std::vector<Token>& out);
    /// Emit a single simple token (Comment, String, Number, etc.).
    void emitSimple(const StyleRange& range, TokenKind kind, std::vector<Token>& out);
    /// Split a whitespace-only byte range into Whitespace / Newline tokens.
    void emitWhitespaceRun(const std::string& text, Sci_PositionU rangeStart, std::vector<Token>& out);

    /// Walks `tokens` once and assigns `Token::line` to each, incrementing
    /// on every Newline. Tokens come out in source order so a single pass
    /// is enough — no per-emit-site instrumentation needed.
    static void stampLines(std::vector<Token>& tokens);

    IStyledSource& m_src;     ///< Source view we read from.
    Sci_PositionU m_pos = 0;  ///< Current scan position.
    bool m_canBeUnary = true; ///< Operator unary/binary disambiguation context.
    /// Index of the open directive token in `out`. SIZE_MAX when no directive
    /// is open (between PP lines / inside non-PP code). The first KeywordPP
    /// run on a line appends to this token; subsequent KeywordPP runs emit
    /// standalone tokens.
    std::size_t m_ppDirectiveIdx = std::numeric_limits<std::size_t>::max();
    std::pair<Sci_PositionU, Sci_PositionU> m_range {}; ///< Active scan range.
    std::string m_lowerKey;                             ///< Reused lowercase buffer for keyword-table lookups.
};

} // namespace fbide::lexer
