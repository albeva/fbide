//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "TokenTransform.hpp"

namespace fbide {

/// Rebuilds indentation based on FreeBASIC block structure.
/// Preprocessor and code blocks form independent nesting trees.
/// Modified strings are stored internally; returned token views are valid
/// as long as this transform is alive.
class ReindentTransform final : public TokenTransform {
public:
    explicit ReindentTransform(const int tabSize, const bool anchorHash = false)
        : m_tabSize(tabSize), m_anchorHash(anchorHash) {}
    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) const -> std::vector<lexer::Token> override;

private:
    int m_tabSize;
    bool m_anchorHash;
    mutable std::vector<std::string> m_pool;
};

} // namespace fbide
