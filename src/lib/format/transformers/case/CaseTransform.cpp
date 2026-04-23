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
    CaseModeInfo { CaseMode::None,  "None"  },
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

auto CaseTransform::apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> {
    std::vector result { tokens };
    if (m_mode == CaseMode::None) {
        return result;
    }

    for (auto& tok : result) {
        if (tok.verbatim || not isKeyword(tok)) {
            continue;
        }
        tok.text = m_mode.apply(std::move(tok.text));
    }
    return result;
}
