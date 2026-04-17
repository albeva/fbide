//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/analyses/lexer/Token.hpp"

namespace fbide {

/// Abstract base for token-to-token transforms. Composable in a pipeline.
/// The returned tokens own their text.
class Transform {
public:
    virtual ~Transform() = default;
    [[nodiscard]] virtual auto apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> = 0;
};

} // namespace fbide
