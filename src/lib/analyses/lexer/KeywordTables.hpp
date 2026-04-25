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

} // namespace fbide::lexer
