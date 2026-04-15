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

/// Keyword case conversion modes.
enum class CaseMode { Mixed, Upper, Lower };

/// Transforms keyword token text to the selected case.
/// Modified strings are stored internally; returned token views are valid
/// as long as this transform is alive.
class CaseTransform final : public TokenTransform {
public:
    explicit CaseTransform(const CaseMode mode) : m_mode(mode) {}
    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) const -> std::vector<lexer::Token> override;

private:
    CaseMode m_mode;
    mutable std::vector<std::string> m_pool;
};

} // namespace fbide
