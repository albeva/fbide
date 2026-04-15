//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReindentTransform.hpp"
using namespace fbide;
using lexer::KeywordKind;

namespace {

struct LineKeywords {
    KeywordKind first = KeywordKind::None;
    KeywordKind second = KeywordKind::None;
    KeywordKind last = KeywordKind::None;
    bool lastAtEnd = false;
    bool hasColon = false;
};

auto getLineKeywords(const std::vector<const lexer::Token*>& lineTokens) -> LineKeywords {
    LineKeywords result;
    bool trailingContent = false;
    for (const auto* tok : lineTokens) {
        if (tok->kind == lexer::TokenKind::Whitespace || tok->kind == lexer::TokenKind::Newline) {
            continue;
        }
        if (tok->kind == lexer::TokenKind::Comment || tok->kind == lexer::TokenKind::CommentBlock) {
            break;
        }
        if (tok->kind == lexer::TokenKind::Operator && tok->text == ":") {
            result.hasColon = true;
        }
        if (tok->keywordKind == KeywordKind::None || tok->keywordKind == KeywordKind::Other) {
            trailingContent = true;
            continue;
        }
        trailingContent = false;
        if (result.first == KeywordKind::None) {
            result.first = tok->keywordKind;
        } else if (result.second == KeywordKind::None) {
            result.second = tok->keywordKind;
        }
        result.last = tok->keywordKind;
    }
    result.lastAtEnd = !trailingContent && result.last != KeywordKind::None;
    return result;
}

auto opensBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::Sub:
        case KeywordKind::Function:
        case KeywordKind::Do:
        case KeywordKind::While:
        case KeywordKind::For:
        case KeywordKind::With:
        case KeywordKind::Scope:
        case KeywordKind::Enum:
        case KeywordKind::Union:
        case KeywordKind::Select:
        case KeywordKind::Asm:
            return true;
        default:
            return false;
    }
}

auto closesBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::End:
        case KeywordKind::Loop:
        case KeywordKind::Next:
        case KeywordKind::Wend:
            return true;
        default:
            return false;
    }
}

/// A line and its trailing newline token (if any).
struct Line {
    std::vector<const lexer::Token*> tokens;
    const lexer::Token* newline = nullptr;
};

} // namespace

auto ReindentTransform::apply(const std::vector<lexer::Token>& tokens) const -> std::vector<lexer::Token> {
    m_pool.clear();
    m_pool.reserve(tokens.size() / 5); // rough estimate: one indent string per ~5 tokens

    // Split into lines, tracking the newline that ends each
    std::vector<Line> lines;
    lines.emplace_back();
    for (const auto& tok : tokens) {
        if (tok.kind == lexer::TokenKind::Newline) {
            lines.back().newline = &tok;
            lines.emplace_back();
        } else {
            lines.back().tokens.push_back(&tok);
        }
    }

    // Build output
    std::vector<lexer::Token> result;
    result.reserve(tokens.size());
    std::size_t indent = 0;

    for (const auto& line : lines) {
        // Find first non-whitespace token
        auto contentStart = line.tokens.begin();
        while (contentStart != line.tokens.end() && (*contentStart)->kind == lexer::TokenKind::Whitespace) {
            ++contentStart;
        }

        if (contentStart != line.tokens.end()) {
            const auto kws = getLineKeywords(line.tokens);
            const bool isPreprocessor = (*contentStart)->kind == lexer::TokenKind::Preprocessor;

            bool dedentBefore = false;
            bool indentAfter = false;

            if (!isPreprocessor && !kws.hasColon) {
                if (closesBlock(kws.first)) {
                    dedentBefore = true;
                } else if (kws.first == KeywordKind::Case || kws.first == KeywordKind::ElseIf) {
                    dedentBefore = true;
                    indentAfter = true;
                } else if (kws.first == KeywordKind::Else && kws.lastAtEnd) {
                    dedentBefore = true;
                    indentAfter = true;
                } else if (kws.first == KeywordKind::If && kws.last == KeywordKind::Then && kws.lastAtEnd) {
                    indentAfter = true;
                } else if (kws.first == KeywordKind::Type && kws.second != KeywordKind::As) {
                    indentAfter = true;
                } else if (opensBlock(kws.first)) {
                    indentAfter = true;
                }
            }

            if (dedentBefore && indent > 0) {
                indent--;
            }

            // Emit indentation
            if (!isPreprocessor && indent > 0) {
                m_pool.emplace_back(indent * static_cast<std::size_t>(m_tabSize), ' ');
                result.push_back({ lexer::TokenKind::Whitespace, lexer::KeywordKind::None, m_pool.back() });
            }

            // Emit content tokens (skip leading whitespace)
            for (auto it = contentStart; it != line.tokens.end(); ++it) {
                result.push_back(**it);
            }

            if (indentAfter) {
                indent++;
            }
        }

        // Emit trailing newline
        if (line.newline != nullptr) {
            result.push_back(*line.newline);
        }
    }

    return result;
}
