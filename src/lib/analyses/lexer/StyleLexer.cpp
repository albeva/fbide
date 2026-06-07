//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "StyleLexer.hpp"
#include <array>
#include "KeywordTables.hpp"
#include "VerbatimAnnotator.hpp"
#include "config/ThemeCategory.hpp"
#include "config/Value.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
using namespace fbide;
using namespace fbide::lexer;

void lexer::setFbKeywords(const Value& kw) {
    std::array<std::string, kThemeKeywordGroupsCount> groups;
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[idx]);
        groups[idx] = std::string(kw.get_or(wxString(key), "").utf8_str());
    }
    FBSciLexer::setKeywords(groups);
}

namespace {

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

/// Lowercase `sv` into `out`, reusing its capacity (no per-call alloc).
void toLowerInto(std::string& out, const std::string_view sv) {
    out.resize(sv.size());
    for (std::size_t i = 0; i < sv.size(); i++) {
        out[i] = asciiLower(sv[i]);
    }
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
    // Reserve from the resolved scan range, not the whole document: a
    // single-line lex (e.g. AutoIndent on every Enter) in a large file
    // would otherwise malloc whole-document capacity for a few tokens.
    tokens.reserve(std::max<Sci_PositionU>((m_range.second - m_range.first) / 5, 16));

    m_pos = m_range.first;
    m_canBeUnary = true;
    m_ppDirectiveIdx = std::numeric_limits<std::size_t>::max();

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
    // Clamp to the requested range, not the whole document: a style run
    // that crosses rangeEnd must not coalesce past it, or a sub-range lex
    // (paste / on-type transform) would write beyond the intended range.
    while (m_pos < m_range.second && m_src.styleAt(m_pos) == style) {
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
    case OperatorPP:
        emitOperator(range, out);
        break;
    case Identifier:
    case IdentifierPP:
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
        emitKeywordPP(range, out);
        break;
    case Preprocessor:
        emitPreprocessor(range, out);
        break;
    case Number:
        emitSimple(range, TokenKind::Number, out);
        break;
    case NumberPP:
        emitSimple(range, TokenKind::Number, out);
        break;
    case String:
        emitSimple(range, TokenKind::String, out);
        break;
    case StringPP:
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
                // CR transferred — drop the now-empty Whitespace token so
                // the merged Newline cleanly replaces the bare-`\r` artifact.
                if (out.back().text.empty() && out.back().kind == TokenKind::Whitespace) {
                    out.pop_back();
                }
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
            // PP line ends — clear the open-directive marker so the next
            // line's `#` opens a fresh directive token. Continuation lines
            // are handled by the lexer continuing in Preprocessor state, so
            // we still get a fresh Preprocessor run for the next physical
            // line; resetting here is safe because that run is also bare-`#`
            // -free (whitespace only).
            m_ppDirectiveIdx = std::numeric_limits<std::size_t>::max();
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
            range.style,
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
    toLowerInto(m_lowerKey, text);
    auto kwKind = KeywordKind::Other;
    const auto& kw = structuralKeywords();
    if (const auto it = kw.find(m_lowerKey); it != kw.end()) {
        kwKind = it->second;
    }
    // The structural classifier maps every `asm` to `KeywordKind::Asm`, but
    // the lexer's per-line AsmState distinguishes a block opener (Block)
    // from a single-line statement (Stmt → resolved to None at logical EOL).
    // Downgrade non-block asm to `Other` so neither AutoIndent nor the
    // formatter treat it as a compound-statement opener. Walk forward through
    // continuation / mid-comment lines (Undetermined) to find resolution.
    if (kwKind == KeywordKind::Asm) {
        auto line = m_src.lineFromPosition(r.start);
        const auto lastLine = m_src.lineFromPosition(m_src.length());
        constexpr int kMaxScan = 100;
        auto resolved = FBSciLexer::AsmState::Undetermined;
        for (int i = 0; i < kMaxScan && line <= lastLine; i++, line++) {
            const auto state = m_src.lineState(line).asmState;
            if (state != FBSciLexer::AsmState::Undetermined) {
                resolved = state;
                break;
            }
        }
        if (resolved != FBSciLexer::AsmState::Block) {
            kwKind = KeywordKind::Other;
        }
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

void StyleLexer::emitWhitespaceRun(const std::string& text, const Sci_PositionU rangeStart, std::vector<Token>& out) {
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '\n' || (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')) {
            std::size_t end = i + (c == '\r' ? 2 : 1);
            const auto lineEndPos = rangeStart + static_cast<Sci_PositionU>(i);
            const auto line = m_src.lineFromPosition(lineEndPos);
            const auto continuation = m_src.lineState(line).continueLine;
            out.push_back(Token {
                TokenKind::Newline,
                KeywordKind::None,
                OperatorKind::None,
                ThemeCategory::Default,
                false,
                continuation,
                text.substr(i, end - i),
            });
            m_ppDirectiveIdx = std::numeric_limits<std::size_t>::max();
            i = end;
        } else {
            // Bare `\r` (no trailing `\n` in this run) folds into a following
            // Newline token via emitDefault's CR-transfer logic — emit as
            // Whitespace so the byte survives until the merge happens.
            std::size_t end = i + 1;
            while (end < text.size()
                   && (text[end] == ' ' || text[end] == '\t' || text[end] == '\r')
                   && !(text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n')) {
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
        }
    }
}

void StyleLexer::emitPreprocessor(const StyleRange& range, std::vector<Token>& out) {
    auto text = stringFromRange(range.start, range.end);
    if (text.empty()) {
        return;
    }

    // Run starting with `#` opens a new directive token. Anything else (pure
    // whitespace inter-token gap inside a PP body) emits as Whitespace tokens.
    if (text.front() == '#') {
        m_ppDirectiveIdx = out.size();
        out.push_back(Token {
            TokenKind::Preprocessor,
            KeywordKind::PpOther,
            OperatorKind::None,
            ThemeCategory::Preprocessor,
            false,
            false,
            std::string { text.substr(0, 1) },
        });
        // Trailing whitespace within this run (rare — usually the lexer
        // emits a fresh run for whitespace) — split into Whitespace tokens.
        if (text.size() > 1) {
            emitWhitespaceRun(text.substr(1), range.start + 1, out);
        }
    } else {
        emitWhitespaceRun(text, range.start, out);
    }
    m_canBeUnary = true;
}

void StyleLexer::emitKeywordPP(const StyleRange& range, std::vector<Token>& out) {
    auto text = stringFromRange(range.start, range.end);
    toLowerInto(m_lowerKey, text);
    KeywordKind kwKind = KeywordKind::PpOther;
    const auto& pp = ppKeywords();
    if (const auto it = pp.find(m_lowerKey); it != pp.end()) {
        kwKind = it->second;
    }

    // Append to the in-progress directive token only when it's still the
    // bare `#` AND no intervening tokens have been emitted on the same line
    // — that's the directive-position slot. Otherwise emit as a standalone
    // Preprocessor token (e.g. `once` after `include`, or PP wordlist
    // matches in macro bodies that must not reclassify the directive).
    const bool atDirectivePos = m_ppDirectiveIdx < out.size()
                             && m_ppDirectiveIdx == out.size() - 1
                             && out[m_ppDirectiveIdx].text == "#";
    if (atDirectivePos) {
        out[m_ppDirectiveIdx].text += text;
        out[m_ppDirectiveIdx].keywordKind = kwKind;
        out[m_ppDirectiveIdx].style = ThemeCategory::KeywordPP;
    } else {
        out.push_back(Token {
            TokenKind::Preprocessor,
            kwKind,
            OperatorKind::None,
            ThemeCategory::KeywordPP,
            false,
            false,
            std::move(text),
        });
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
