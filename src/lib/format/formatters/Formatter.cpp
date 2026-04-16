//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Formatter.hpp"
#include "Scanner.hpp"
#include "Renderer.hpp"
using namespace fbide::format;

auto Formatter::format(const std::vector<lexer::Token>& tokens) const -> std::string {
    auto tree = Scanner::scan(tokens);
    Renderer renderer(m_options);
    return renderer.render(tree);
}
