//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CaseTransform.hpp"
using namespace fbide;

namespace {

auto isKeyword(const lexer::Token& tkn) -> bool {
    return lexer::isKeywordToken(tkn.kind);
}

auto asciiUpper(const char ch) -> char {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

struct CaseModeInfo {
    CaseMode mode;
    std::string_view key;
};

constexpr std::array kCaseModeInfo {
    CaseModeInfo { CaseMode::None, "None" },
    CaseModeInfo { CaseMode::Lower, "Lower" },
    CaseModeInfo { CaseMode::Upper, "Upper" },
    CaseModeInfo { CaseMode::Mixed, "Mixed" },
};

} // namespace

auto CaseMode::toString() const -> std::string_view {
    for (const auto& info : kCaseModeInfo) {
        if (info.mode == m_mode) {
            return info.key;
        }
    }
    std::unreachable();
}

auto CaseMode::parse(const std::string_view key) -> std::optional<CaseMode> {
    for (const auto& info : kCaseModeInfo) {
        if (info.key == key) {
            return CaseMode { info.mode };
        }
    }
    return std::nullopt;
}

auto CaseMode::apply(std::string text) const -> std::string {
    switch (m_mode) {
    case None:
        break;
    case Lower:
        for (auto& c : text) {
            c = asciiLower(c);
        }
        break;
    case Upper:
        for (auto& c : text) {
            c = asciiUpper(c);
        }
        break;
    case Mixed:
        if (!text.empty()) {
            text[0] = asciiUpper(text[0]);
            for (std::size_t i = 1; i < text.size(); i++) {
                text[i] = asciiLower(text[i]);
            }
        }
        break;
    }
    return text;
}

auto CaseMode::apply(wxString text) const -> wxString {
    switch (m_mode) {
    case None:
        break;
    case Lower:
        text.MakeLower();
        break;
    case Upper:
        text.MakeUpper();
        break;
    case Mixed:
        text.MakeLower();
        text[0] = wxTolower(text[0]);
        break;
    }
    return text;
}

namespace {

auto isAlpha(const char ch) -> bool {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

/// For a Preprocessor token like "#ifdef FOO", apply `mode` to the directive
/// word only ("ifdef") and leave `#`, whitespace, and the body unchanged.
auto applyToPpDirective(std::string text, const CaseMode mode) -> std::string {
    std::size_t i = 0;
    while (i < text.size() && (text[i] == '#' || text[i] == ' ' || text[i] == '\t')) {
        i++;
    }
    std::size_t end = i;
    while (end < text.size() && isAlpha(text[end])) {
        end++;
    }
    if (end == i) {
        return text;
    }
    auto cased = mode.apply(text.substr(i, end - i));
    text.replace(i, end - i, cased);
    return text;
}

} // namespace

auto CaseTransform::apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> {
    std::vector result { tokens };
    for (auto& tok : result) {
        if (tok.verbatim) {
            continue;
        }
        if (isKeyword(tok)) {
            const auto mode = m_cases[indexOfKeywordGroup(tok.style)];
            if (mode == CaseMode::None) {
                continue;
            }
            tok.text = mode.apply(std::move(tok.text));
        } else if (tok.kind == lexer::TokenKind::Preprocessor) {
            // PP token text is "#<dirword>[ <body>]". Use KeywordPP's case for
            // the directive word, body stays as-is.
            const auto mode = m_cases[indexOfKeywordGroup(ThemeCategory::KeywordPP)];
            if (mode == CaseMode::None) {
                continue;
            }
            tok.text = applyToPpDirective(std::move(tok.text), mode);
        }
    }
    return result;
}
