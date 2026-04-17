//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Renderer.hpp"
using namespace fbide;

auto Renderer::render(
    const std::vector<lexer::Token>& tokens,
    const std::vector<std::unique_ptr<TokenTransform>>& transforms
) const -> wxString {
    if (transforms.empty()) {
        return render(tokens);
    }

    std::vector<lexer::Token> transformed;
    for (const auto& transformer : transforms) {
        transformed = transformer->apply(transformed.empty() ? tokens : transformed);
    }

    return render(transformed);
}
