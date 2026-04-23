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

/// Keyword case conversion mode. Thin wrapper around the nested `Value`
/// enum — implicit conversion from/to `Value` makes it interchangeable
/// with plain enum constants (`CaseMode m = CaseMode::Lower`).
class CaseMode {
public:
    enum Value : std::uint8_t {
        None,  ///< Pass-through — do not transform.
        Lower, ///< all lowercase
        Upper, ///< ALL UPPERCASE
        Mixed, ///< Capitalised — first char upper, rest lower
    };

    constexpr CaseMode(const Value v) noexcept
    : m_mode(v) {}

    constexpr operator Value() const noexcept { return m_mode; }
    [[nodiscard]] constexpr auto value() const noexcept -> Value { return m_mode; }

    friend constexpr auto operator==(const CaseMode&, const CaseMode&) noexcept -> bool = default;
    friend constexpr auto operator==(const CaseMode lhs, const Value rhs) noexcept -> bool { return lhs.m_mode == rhs; }

    /// All values in stable order — handy for settings dropdowns.
    static constexpr std::array<Value, 4> all { None, Lower, Upper, Mixed };

    /// Stable INI key.
    [[nodiscard]] auto toString() const -> std::string_view;

    /// Parse a stable INI key. Returns nullopt if unknown.
    [[nodiscard]] static auto parse(std::string_view key) -> std::optional<CaseMode>;

    /// Transform `text` according to this mode. `None` returns the input
    /// unchanged. `Mixed` uppercases the first byte and lowercases the
    /// rest. ASCII only.
    [[nodiscard]] auto apply(std::string text) const -> std::string;

private:
    Value m_mode;
};

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
