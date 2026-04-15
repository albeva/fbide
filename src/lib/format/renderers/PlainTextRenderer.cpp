//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "PlainTextRenderer.hpp"
using namespace fbide;

auto PlainTextRenderer::render(const std::vector<lexer::Token>& tokens) const -> wxString {
    wxString output;
    for (const auto& tok : tokens) {
        output += tok.text;
    }
    return output;
}
