//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "FormatTree.hpp"

namespace fbide::format {

/// Code formatter: takes lexer tokens and produces formatted source code.
/// Pipeline: tokens → Scanner → TreeBuilder → Renderer → string.
class Formatter final {
public:
    explicit Formatter(const FormatOptions& options)
        : m_options(options) {}

    /// Format the token stream and return the formatted source code.
    [[nodiscard]] auto format(const std::vector<lexer::Token>& tokens) const -> std::string;

private:
    FormatOptions m_options;
};

} // namespace fbide::format
