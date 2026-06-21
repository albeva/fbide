//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "FormatOptions.hpp"
#include "analyses/parser/TreeParser.hpp"
#include "format/transformers/Transform.hpp"

namespace fbide::reformat {

/// Reformat transform: scans a lexer token stream into a parse tree and
/// renders the tree back as a formatted token stream. Composable in a
/// TokenTransform pipeline.
///
/// Reusable: each apply() re-scans and re-renders from the pinned options.
class ReFormatter final : public Transform {
public:
    /// Construct with format options pinned for the lifetime of `apply`.
    explicit ReFormatter(const FormatOptions& options)
    : m_options(options)
    , m_parser(parser::ParseOptions { .reFormat = options.reFormat }) {}

    /// Apply the reformat — tokens in, formatted tokens out.
    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> override;

private:
    FormatOptions m_options;     ///< Format options pinned for this run.
    parser::TreeParser m_parser; ///< Scans the token stream into the tree to render.
};

} // namespace fbide::reformat
