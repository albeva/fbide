//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Renderer.hpp"
using namespace fbide;

auto Renderer::render(
    const std::vector<Token>& tokens,
    const std::vector<std::unique_ptr<TokenTransform>>& transforms
) const -> wxString {
    if (transforms.empty()) {
        return render(tokens);
    }

    // Apply transforms in sequence
    auto transformed = transforms.front()->apply(tokens);
    for (std::size_t i = 1; i < transforms.size(); i++) {
        transformed = transforms[i]->apply(std::move(transformed));
    }
    return render(transformed);
}
