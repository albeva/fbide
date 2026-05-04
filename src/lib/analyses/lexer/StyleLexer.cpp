//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "StyleLexer.hpp"
// clang-format off
#include "ILexer.h"
// clang-format on
#include "KeywordTables.hpp"
#include "VerbatimAnnotator.hpp"
#include "config/ThemeCategory.hpp"
#include "config/Value.hpp"
using namespace fbide;
using namespace fbide::lexer;

void lexer::configureFbWordlists(Scintilla::ILexer5& lex, const Value& kw) {
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[idx]);
        const auto words = kw.get_or(wxString(key), "").Lower();
        lex.WordListSet(static_cast<int>(idx), words.utf8_str());
    }
}

namespace {

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

auto toLower(const std::string_view sv) -> std::string {
    std::string r(sv.size(), '\0');
    for (std::size_t i = 0; i < sv.size(); i++) {
        r[i] = asciiLower(sv[i]);
    }
    return r;
}

/// Longest-match operator dispatch via first-char switch.
/// Operators outside the table collapse to `OperatorKind::Other`, one byte
/// consumed. Multi-char operators not enumerated split into N×Other tokens —
/// downstream consumers only branch on the kinds enumerated here.
auto matchOperator(const std::string_view slice) -> std::pair<OperatorKind, std::size_t> {
    using enum OperatorKind;
    if (slice.empty()) {
        return { Other, 0 };
    }
    const auto peek = [&](const std::size_t i) -> char {
        return i < slice.size() ? slice[i] : '\0';
    };
    switch (slice[0]) {
    case '-':
        if (peek(1) == '>')
            return { Arrow, 2 };
        if (peek(1) == '=')
            return { Other, 2 }; // -=
        return { Subtract, 1 };
    case '+':
        if (peek(1) == '=')
            return { Other, 2 }; // +=
        return { Add, 1 };
    case '*':
        if (peek(1) == '=')
            return { Other, 2 }; // *=
        return { Multiply, 1 };
    case '/':
        if (peek(1) == '=')
            return { Other, 2 }; // /=
        return { Other, 1 };
    case '\\':
        if (peek(1) == '=')
            return { Other, 2 }; // \=
        return { Other, 1 };
    case '^':
        if (peek(1) == '=')
            return { Other, 2 }; // ^=
        return { Other, 1 };
    case '&':
        if (peek(1) == '=')
            return { Other, 2 }; // &=
        return { Other, 1 };
    case '<':
        if (slice.starts_with("<="))
            return { Other, 2 };
        if (slice.starts_with("<>"))
            return { Other, 2 };
        return { Other, 1 };
    case '>':
        if (slice.starts_with(">="))
            return { Other, 2 };
        return { Other, 1 };
    case '=':
        return { Assign, 1 };
    case '.':
        if (slice.starts_with("..."))
            return { Ellipsis3, 3 };
        return { Dot, 1 };
    case ',':
        return { Comma, 1 };
    case ';':
        return { Semicolon, 1 };
    case ':':
        return { Colon, 1 };
    case '?':
        return { Question, 1 };
    case '(':
        return { ParenOpen, 1 };
    case ')':
        return { ParenClose, 1 };
    case '[':
        return { BracketOpen, 1 };
    case ']':
        return { BracketClose, 1 };
    case '{':
        return { BraceOpen, 1 };
    case '}':
        return { BraceClose, 1 };
    case '@':
        return { AddressOf, 1 };
    default:
        return { Other, 1 };
    }
}

} // namespace

StyleLexer::StyleLexer(IStyledSource& src)
: m_src(src)
, m_range(0, src.length()) {}

void StyleLexer::tokenise(std::vector<Token>& tokens, const Range& range) {
    const auto restore = ValueRestorer { m_range };
    m_range.first = range.first == 0 ? m_range.first : range.first;
    m_range.second = range.second == 0 ? m_range.second : range.second;

    tokens.clear();
    tokens.reserve(m_src.length() / 5);

    m_pos = m_range.first;
    m_canBeUnary = true;
    m_inPpLine = false;
    m_ppTokenIdx = 0;

    while (auto r = nextStyle()) {
        emitFromRange(*r, tokens);
    }

    annotateVerbatim(tokens);
    stampLines(tokens);
}

void StyleLexer::stampLines(std::vector<Token>& tokens) {
    int line = 0;
    for (auto& tkn : tokens) {
        tkn.line = line;
        if (tkn.kind == TokenKind::Newline) {
            line++;
        }
    }
}

auto StyleLexer::nextStyle() -> std::optional<StyleRange> {
    if (m_pos >= m_range.second) {
        return std::nullopt;
    }
    const auto style = m_src.styleAt(m_pos);
    const auto start = m_pos;
    while (m_pos < m_src.length() && m_src.styleAt(m_pos) == style) {
        m_pos++;
    }
    return StyleRange { style, start, m_pos };
}

auto StyleLexer::stringFromRange(const Sci_PositionU start, const Sci_PositionU end) const -> std::string {
    if (end <= start) {
        return {};
    }
    std::string s(end - start, '\0');
    m_src.getCharRange(s.data(), start, end - start);
    return s;
}

void StyleLexer::emitFromRange(const StyleRange& range, std::vector<Token>& out) {
    using enum ThemeCategory;
    switch (range.style) {
    case Default:
        emitDefault(range, out);
        break;
    case Operator:
        emitOperator(range, out);
        break;
    case Identifier:
        emitIdentifier(range, out);
        break;
    case Keywords:
        emitKeyword(range, TokenKind::Keywords, out);
        break;
    case KeywordTypes:
        emitKeyword(range, TokenKind::KeywordTypes, out);
        break;
    case KeywordOperators:
        emitKeyword(range, TokenKind::KeywordOperators, out);
        break;
    case KeywordConstants:
        emitKeyword(range, TokenKind::KeywordConstants, out);
        break;
    case KeywordLibrary:
        emitKeyword(range, TokenKind::KeywordLibrary, out);
        break;
    case KeywordCustom:
        emitKeyword(range, TokenKind::KeywordCustom, out);
        break;
    case KeywordAsm1:
        emitKeyword(range, TokenKind::KeywordAsm1, out);
        break;
    case KeywordAsm2:
        emitKeyword(range, TokenKind::KeywordAsm2, out);
        break;
    case KeywordPP:
        emitPreprocessor(range, out);
        break;
    case Preprocessor:
        emitPreprocessor(range, out);
        break;
    case Number:
        emitSimple(range, TokenKind::Number, out);
        break;
    case String:
        emitSimple(range, TokenKind::String, out);
        break;
    case StringOpen:
        emitSimple(range, TokenKind::UnterminatedString, out);
        break;
    case Comment:
        emitSimple(range, TokenKind::Comment, out);
        break;
    case MultilineComment:
        emitSimple(range, TokenKind::CommentBlock, out);
        break;
    case Label:
        emitSimple(range, TokenKind::Identifier, out);
        break;
    case Error:
        emitSimple(range, TokenKind::Invalid, out);
        break;
    }
}

void StyleLexer::emitDefault(const StyleRange& r, std::vector<Token>& out) {
    const auto text = stringFromRange(r.start, r.end);
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '\n' || c == '\r') {
            std::size_t end = i + 1;
            if (c == '\r' && end < text.size() && text[end] == '\n') {
                end++;
            }
            // FBSciLexer keeps a `\r` (of CRLF) inside the previous styled run
            // (Comment / Preprocessor / etc.). Move it onto the Newline token
            // so HTML / formatter output doesn't wrap it inside a styled span.
            std::string newlineText = text.substr(i, end - i);
            if (c == '\n' && !out.empty() && !out.back().text.empty()
                && out.back().text.back() == '\r') {
                out.back().text.pop_back();
                newlineText = "\r" + newlineText;
            }
            // Mark continuation: query FBSciLexer's per-line state for the line
            // that this newline ends. continueLine=true means a `_` marker on
            // the source line — the formatter must keep the logical statement
            // open across this newline.
            const auto lineEndPos = r.start + static_cast<Sci_PositionU>(i);
            const auto line = m_src.lineFromPosition(lineEndPos);
            const auto continuation = m_src.lineState(line).continueLine;
            out.push_back(Token {
                TokenKind::Newline,
                KeywordKind::None,
                OperatorKind::None,
                ThemeCategory::Default,
                false,
                continuation,
                std::move(newlineText),
            });
            m_canBeUnary = true;
            m_inPpLine = false;
            i = end;
        } else if (c == ' ' || c == '\t') {
            std::size_t end = i + 1;
            while (end < text.size() && (text[end] == ' ' || text[end] == '\t')) {
                end++;
            }
            out.push_back(Token {
                TokenKind::Whitespace,
                KeywordKind::None,
                OperatorKind::None,
                ThemeCategory::Default,
                false,
                false,
                text.substr(i, end - i),
            });
            i = end;
        } else {
            // Unexpected non-whitespace under Default style — treat as Invalid.
            out.push_back(Token {
                TokenKind::Invalid,
                KeywordKind::None,
                OperatorKind::None,
                ThemeCategory::Default,
                false,
                false,
                std::string(1, c),
            });
            i++;
        }
    }
}

void StyleLexer::emitOperator(const StyleRange& range, std::vector<Token>& out) {
    const auto text = stringFromRange(range.start, range.end);
    const std::string_view sv { text };
    std::size_t i = 0;
    while (i < sv.size()) {
        auto [op, len] = matchOperator(sv.substr(i));
        // Unary form fixups
        if (m_canBeUnary) {
            if (op == OperatorKind::Add) {
                op = OperatorKind::UnaryPlus;
            } else if (op == OperatorKind::Subtract) {
                op = OperatorKind::Negate;
            } else if (op == OperatorKind::Multiply) {
                op = OperatorKind::Dereference;
            }
        }
        out.push_back(Token {
            TokenKind::Operator,
            KeywordKind::None,
            op,
            ThemeCategory::Operator,
            false,
            false,
            std::string { sv.substr(i, len) },
        });
        // Update unary context
        switch (op) {
        case OperatorKind::ParenClose:
        case OperatorKind::BracketClose:
        case OperatorKind::BraceClose:
            m_canBeUnary = false;
            break;
        default:
            m_canBeUnary = true;
            break;
        }
        i += len;
    }
}

void StyleLexer::emitIdentifier(const StyleRange& r, std::vector<Token>& out) {
    out.push_back(Token {
        TokenKind::Identifier,
        KeywordKind::None,
        OperatorKind::None,
        r.style,
        false,
        false,
        stringFromRange(r.start, r.end),
    });
    m_canBeUnary = false;
}

void StyleLexer::emitKeyword(const StyleRange& r, TokenKind kind, std::vector<Token>& out) {
    auto text = stringFromRange(r.start, r.end);
    const auto lower = toLower(text);
    auto kwKind = KeywordKind::Other;
    const auto& kw = structuralKeywords();
    if (const auto it = kw.find(lower); it != kw.end()) {
        kwKind = it->second;
    }
    out.push_back(Token {
        kind,
        kwKind,
        OperatorKind::None,
        r.style,
        false,
        false,
        std::move(text),
    });
    m_canBeUnary = true; // after a keyword (And, Not, If, ...) next operator is unary
}

void StyleLexer::emitPreprocessor(const StyleRange& range, std::vector<Token>& out) {
    auto text = stringFromRange(range.start, range.end);
    // Snapshot the token text length *before* appending this run so we can
    // tell directive-position runs apart from body runs below.
    const std::size_t prevLen = m_inPpLine ? out[m_ppTokenIdx].text.size() : 0;

    if (!m_inPpLine) {
        // Start a new PP token spanning this run.
        m_inPpLine = true;
        m_ppTokenIdx = out.size();
        out.push_back(Token {
            TokenKind::Preprocessor,
            KeywordKind::PpOther,
            OperatorKind::None,
            ThemeCategory::Preprocessor,
            false,
            false,
            std::move(text),
        });
    } else {
        // Append to the in-progress PP token.
        out[m_ppTokenIdx].text += text;
    }
    // KeywordPP carries the directive word — fill kwKind and tag style.
    // FBSciLexer styles `#` first as Preprocessor, then re-enters and styles
    // the directive word as KeywordPP, so this branch can fire either as the
    // first range of the PP token (rare — would mean `#directive` was a
    // single KeywordPP run) or as the second range right after a bare `#`
    // Preprocessor run.
    //
    // Only the directive position counts. FBSciLexer also paints body words
    // that happen to match the KeywordPP wordlist (e.g. `If`, `else`,
    // `endif`) as KeywordPP, so without this guard `#define VT_GUARD If x
    // Then ...` would be re-classified as a `PpIf` block opener and confuse
    // the formatter / scope tracker.
    if (range.style == ThemeCategory::KeywordPP && prevLen <= 1) {
        std::string runText = stringFromRange(range.start, range.end);
        // Strip a leading `#` for the lookup — happens only when the entire
        // `#directive` is one KeywordPP run (no preceding bare `#` range).
        if (!runText.empty() && runText.front() == '#') {
            runText.erase(0, 1);
        }
        const auto runLower = toLower(runText);
        const auto& pp = ppKeywords();
        if (const auto it = pp.find(runLower); it != pp.end()) {
            out[m_ppTokenIdx].keywordKind = it->second;
        }
        out[m_ppTokenIdx].style = ThemeCategory::KeywordPP;
    }
    m_canBeUnary = true;
}

void StyleLexer::emitSimple(const StyleRange& range, const TokenKind kind, std::vector<Token>& out) {
    out.push_back(Token {
        kind,
        KeywordKind::None,
        OperatorKind::None,
        range.style,
        false,
        false,
        stringFromRange(range.start, range.end),
    });
    // After a value-producing token (number/string/identifier/label) next
    // operator is binary. After a comment the surrounding state is unchanged
    // — but we conservatively treat CommentBlock as unary-eligible since the
    // legacy lexer does (Lexer.cpp:212).
    switch (kind) {
    case TokenKind::Number:
    case TokenKind::String:
    case TokenKind::UnterminatedString:
    case TokenKind::Identifier:
        m_canBeUnary = false;
        break;
    case TokenKind::CommentBlock:
        m_canBeUnary = true;
        break;
    default:
        break;
    }
}
