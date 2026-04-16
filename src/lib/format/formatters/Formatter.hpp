//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/analyses/lexer/Token.hpp"

namespace fbide::format {

/// Code formatter: takes lexer tokens and produces formatted source code.
/// Pipeline: tokens → Scanner → TreeBuilder → Renderer → string.
class Formatter final {
public:
    explicit Formatter(const std::size_t tabSize, const bool anchorHash = false)
        : m_tabSize(tabSize)
        , m_anchorHash(anchorHash) {}

    /// Format the token stream and return the formatted source code.
    [[nodiscard]] auto format(const std::vector<lexer::Token>& tokens) const -> std::string;

private:
    std::size_t m_tabSize;
    bool m_anchorHash;
};

} // namespace fbide::format
