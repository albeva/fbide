//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "../Token.hpp"

namespace fbide {

/// Abstract base for token-to-token transforms. Composable in a pipeline.
class TokenTransform {
public:
    virtual ~TokenTransform() = default;
    [[nodiscard]] virtual auto apply(std::vector<Token> tokens) const -> std::vector<Token> = 0;
};

} // namespace fbide
