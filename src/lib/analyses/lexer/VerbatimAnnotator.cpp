//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "VerbatimAnnotator.hpp"
#include <string_view>
using namespace fbide::lexer;

namespace {

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

auto isWordChar(const char ch) -> bool {
    if (ch >= 'a' && ch <= 'z')
        return true;
    if (ch >= 'A' && ch <= 'Z')
        return true;
    if (ch >= '0' && ch <= '9')
        return true;
    return ch == '_' || ch == '$';
}

enum class PragmaKind { None,
    Off,
    On };

void skipSpaces(const std::string_view text, std::size_t& pos) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }
}

auto matchWordIgnoreCase(const std::string_view text, std::size_t& pos, const std::string_view word) -> bool {
    if (text.size() - pos < word.size()) {
        return false;
    }
    for (std::size_t i = 0; i < word.size(); i++) {
        if (asciiLower(text[pos + i]) != word[i]) {
            return false;
        }
    }
    const auto next = pos + word.size();
    if (next < text.size() && isWordChar(text[next])) {
        return false;
    }
    pos = next;
    return true;
}

auto classifyCommentPragma(const std::string_view text) -> PragmaKind {
    std::size_t pos = 0;
    if (pos < text.size() && text[pos] == '\'') {
        pos++;
    } else if (!matchWordIgnoreCase(text, pos, "rem")) {
        return PragmaKind::None;
    }

    skipSpaces(text, pos);
    if (!matchWordIgnoreCase(text, pos, "format")) {
        return PragmaKind::None;
    }
    skipSpaces(text, pos);

    PragmaKind kind;
    if (matchWordIgnoreCase(text, pos, "off")) {
        kind = PragmaKind::Off;
    } else if (matchWordIgnoreCase(text, pos, "on")) {
        kind = PragmaKind::On;
    } else {
        return PragmaKind::None;
    }

    skipSpaces(text, pos);
    if (pos != text.size()) {
        return PragmaKind::None;
    }
    return kind;
}

} // namespace

void fbide::lexer::annotateVerbatim(std::vector<Token>& tokens) {
    int depth = 0;
    std::size_t i = 0;
    while (i < tokens.size()) {
        std::size_t lineEnd = i;
        while (lineEnd < tokens.size() && tokens[lineEnd].kind != TokenKind::Newline) {
            lineEnd++;
        }
        const std::size_t lineStop = (lineEnd < tokens.size()) ? lineEnd + 1 : lineEnd;

        PragmaKind kind = PragmaKind::None;
        std::size_t significantCount = 0;
        std::size_t commentIndex = 0;
        for (std::size_t j = i; j < lineEnd; j++) {
            if (tokens[j].kind == TokenKind::Whitespace) {
                continue;
            }
            significantCount++;
            if (significantCount == 1 && tokens[j].kind == TokenKind::Comment) {
                commentIndex = j;
            }
        }
        if (significantCount == 1 && tokens[commentIndex].kind == TokenKind::Comment) {
            kind = classifyCommentPragma(tokens[commentIndex].text);
        }

        const int before = depth;
        int after = depth;
        if (kind == PragmaKind::Off) {
            after = depth + 1;
        } else if (kind == PragmaKind::On && depth > 0) {
            after = depth - 1;
        }
        depth = after;

        if (before > 0 || after > 0) {
            for (std::size_t j = i; j < lineStop; j++) {
                tokens[j].verbatim = true;
            }
        }

        i = lineStop;
    }
}
