//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <string>
#include <unordered_map>
#include "Token.hpp"

namespace fbide::lexer {

/// Map of structurally significant FB keywords (lowercase) to their KeywordKind.
/// Used by both the legacy and new lexer to fill `Token::keywordKind` from text.
[[nodiscard]] auto structuralKeywords() -> const std::unordered_map<std::string, KeywordKind>&;

/// Map of preprocessor directive words (lowercase, without leading `#`) to KeywordKind.
[[nodiscard]] auto ppKeywords() -> const std::unordered_map<std::string, KeywordKind>&;

/// Space-separated lowercase list of all structural keywords. Used to seed an
/// FBSciLexer wordlist when running headless for AutoIndent / formatter — the
/// user's editor wordlist config is irrelevant for block detection, only
/// structural classification matters.
[[nodiscard]] auto structuralKeywordsList() -> const std::string&;

} // namespace fbide::lexer
