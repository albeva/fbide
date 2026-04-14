//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReindentTransform.hpp"
using namespace fbide;

namespace {

auto isKeyword(const TokenKind kind) -> bool {
    return kind == TokenKind::Keyword1
        || kind == TokenKind::Keyword2
        || kind == TokenKind::Keyword3
        || kind == TokenKind::Keyword4;
}

auto keywordText(const Token& tok) -> wxString {
    if (!isKeyword(tok.kind)) {
        return {};
    }
    return tok.text.Upper();
}

struct LineKeywords {
    wxString first;
    wxString second;
    wxString last;
};

auto getLineKeywords(const std::vector<Token*>& lineTokens) -> LineKeywords {
    LineKeywords result;
    for (const auto* tok : lineTokens) {
        if (tok->kind == TokenKind::Whitespace || tok->kind == TokenKind::Newline) {
            continue;
        }
        if (tok->kind == TokenKind::Comment) {
            break;
        }
        const auto kw = keywordText(*tok);
        if (kw.empty()) {
            continue;
        }
        if (result.first.empty()) {
            result.first = kw;
        } else if (result.second.empty()) {
            result.second = kw;
        }
        result.last = kw;
    }
    return result;
}

auto opensBlock(const wxString& kw) -> bool {
    return kw == "SUB" || kw == "FUNCTION" || kw == "DO"
        || kw == "WHILE" || kw == "FOR" || kw == "WITH"
        || kw == "SCOPE" || kw == "ENUM" || kw == "UNION"
        || kw == "SELECT" || kw == "ASM";
}

auto closesBlock(const wxString& kw) -> bool {
    return kw == "END" || kw == "LOOP" || kw == "NEXT"
        || kw == "WEND";
}

auto isMidBlock(const wxString& kw) -> bool {
    return kw == "ELSE" || kw == "ELSEIF" || kw == "CASE";
}

} // namespace

auto ReindentTransform::apply(std::vector<Token> tokens) const -> std::vector<Token> {
    // Split tokens into lines (pointers into the tokens vector)
    std::vector<std::vector<Token*>> lines;
    lines.emplace_back();

    for (auto& tok : tokens) {
        if (tok.kind == TokenKind::Newline) {
            lines.emplace_back();
        } else {
            lines.back().push_back(&tok);
        }
    }

    // Process each line: strip leading whitespace, compute new indentation
    std::size_t indent = 0;

    for (auto& line : lines) {
        // Strip leading whitespace tokens
        while (!line.empty() && line.front()->kind == TokenKind::Whitespace) {
            line.front()->text.clear();
            line.erase(line.begin());
        }

        if (line.empty()) {
            continue;
        }

        const auto kws = getLineKeywords(line);

        // Preprocessor lines: flush to column 0
        if (line.front()->kind == TokenKind::Preprocessor) {
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
        } else if (kws.first == "IF" && kws.last == "THEN") {
            indentAfter = true;
        } else if (kws.first == "TYPE" && kws.second != "AS") {
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
    std::erase_if(tokens, [](const Token& tok) {
        return tok.kind == TokenKind::Whitespace && tok.text.empty();
    });

    return tokens;
}
