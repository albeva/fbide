//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "format/transformers/Transform.hpp"

namespace fbide {

/// Keyword case conversion modes.
enum class CaseMode { Mixed,
    Upper,
    Lower };

/// Transforms keyword token text to the selected case.
class CaseTransform final : public Transform {
public:
    explicit CaseTransform(const CaseMode mode)
    : m_mode(mode) {}

    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> override;

private:
    CaseMode m_mode;
};

} // namespace fbide
