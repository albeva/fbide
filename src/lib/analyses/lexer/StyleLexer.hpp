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

namespace Scintilla {
class ILexer5;
}

namespace fbide {
class Value;
}

namespace fbide::lexer {

/// Apply each keyword group from `kw` (a config `Value` map keyed by
/// `ThemeCategory` name) to the corresponding FBSciLexer wordlist slot.
/// Keywords are lowercased so FBSciLexer's case-insensitive lookup matches.
void configureFbWordlists(Scintilla::ILexer5& lex, const Value& kw);


/// One coalesced run of bytes carrying the same FBSciLexer style.
struct StyleRange {
    ThemeCategory style;
    Sci_PositionU start;
    Sci_PositionU end; // exclusive
};

/// Token producer that walks FBSciLexer-emitted style runs over an
/// `IStyledSource`. Replaces the standalone char-by-char `Lexer` for any
/// caller that has access to a styled document (the editor or a headless
/// `MemoryDocument` driven through `FBSciLexer`).
class StyleLexer final {
public:
    NO_COPY_AND_MOVE(StyleLexer)

    explicit StyleLexer(IStyledSource& src);
    ~StyleLexer() = default;

    [[nodiscard]] auto tokenise() -> std::vector<Token>;

private:
    [[nodiscard]] auto nextStyle() -> std::optional<StyleRange>;
    [[nodiscard]] auto stringFromRange(Sci_PositionU start, Sci_PositionU end) const -> std::string;

    void emitFromRange(const StyleRange& range, std::vector<Token>& out);
    void emitDefault(const StyleRange& range, std::vector<Token>& out);
    void emitOperator(const StyleRange& range, std::vector<Token>& out);
    void emitIdentifier(const StyleRange& range, std::vector<Token>& out);
    void emitKeyword(const StyleRange& range, TokenKind kind, std::vector<Token>& out);
    void emitPreprocessor(const StyleRange& range, std::vector<Token>& out);
    void emitSimple(const StyleRange& range, TokenKind kind, std::vector<Token>& out);

    IStyledSource& m_src;
    Sci_PositionU m_pos = 0;
    bool m_canBeUnary = true;
    bool m_inPpLine = false;
    std::size_t m_ppTokenIdx = 0;
};

} // namespace fbide::lexer
