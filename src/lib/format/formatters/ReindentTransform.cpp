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
};

auto getLineKeywords(const std::vector<lexer::Token*>& lineTokens) -> LineKeywords {
    LineKeywords result;
    for (const auto* tok : lineTokens) {
        if (tok->kind == lexer::TokenKind::Whitespace || tok->kind == lexer::TokenKind::Newline) {
            continue;
        }
        if (tok->kind == lexer::TokenKind::Comment || tok->kind == lexer::TokenKind::CommentBlock) {
            break;
        }
        if (tok->keywordKind == KeywordKind::None || tok->keywordKind == KeywordKind::Other) {
            continue;
        }
        if (result.first == KeywordKind::None) {
            result.first = tok->keywordKind;
        } else if (result.second == KeywordKind::None) {
            result.second = tok->keywordKind;
        }
        result.last = tok->keywordKind;
    }
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

auto isMidBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::Else:
        case KeywordKind::ElseIf:
        case KeywordKind::Case:
            return true;
        default:
            return false;
    }
}

} // namespace

auto ReindentTransform::apply(std::vector<lexer::Token> tokens) const -> std::vector<lexer::Token> {
    // Split tokens into lines (pointers into the tokens vector)
    std::vector<std::vector<lexer::Token*>> lines;
    lines.emplace_back();

    for (auto& tok : tokens) {
        if (tok.kind == lexer::TokenKind::Newline) {
            lines.emplace_back();
        } else {
            lines.back().push_back(&tok);
        }
    }

    // Process each line: strip leading whitespace, compute new indentation
    std::size_t indent = 0;

    for (auto& line : lines) {
        // Strip leading whitespace tokens
        while (!line.empty() && line.front()->kind == lexer::TokenKind::Whitespace) {
            line.front()->text.clear();
            line.erase(line.begin());
        }

        if (line.empty()) {
            continue;
        }

        const auto kws = getLineKeywords(line);

        // Preprocessor lines: flush to column 0
        if (line.front()->kind == lexer::TokenKind::Preprocessor) {
            continue;
        }

        // Determine indent adjustments
        bool dedentBefore = false;
        bool indentAfter = false;

        if (closesBlock(kws.first)) {
            dedentBefore = true;
        } else if (isMidBlock(kws.first)) {
            dedentBefore = true;
            indentAfter = true;
        } else if (kws.first == KeywordKind::If && kws.last == KeywordKind::Then) {
            indentAfter = true;
        } else if (kws.first == KeywordKind::Type && kws.second != KeywordKind::As) {
            indentAfter = true;
        } else if (opensBlock(kws.first)) {
            indentAfter = true;
        }

        if (dedentBefore && indent > 0) {
            indent--;
        }

        // Set leading whitespace on the first token
        if (indent > 0) {
            line.front()->text = wxString(' ', indent * static_cast<std::size_t>(m_tabSize)) + line.front()->text;
        }

        if (indentAfter) {
            indent++;
        }
    }

    // Remove tokens that were cleared (empty text whitespace)
    std::erase_if(tokens, [](const lexer::Token& tok) {
        return tok.kind == lexer::TokenKind::Whitespace && tok.text.empty();
    });

    return tokens;
}
