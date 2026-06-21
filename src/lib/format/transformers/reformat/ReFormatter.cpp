//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReFormatter.hpp"
#include "Renderer.hpp"
using namespace fbide::reformat;
using namespace fbide::lexer;

auto ReFormatter::apply(const std::vector<Token>& tokens) -> std::vector<Token> {
    const auto tree = m_parser.parse(tokens);
    Renderer renderer(m_options);
    return renderer.render(tree);
}
