//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppNonExplicitConvertingConstructor
// ReSharper disable CppNonExplicitConversionOperator
#pragma once
#include "pch.hpp"

namespace fbide {

/// Text file encodings supported by the editor.
/// Thin wrapper around the nested `Value` enum — holds a single `m_encoding`
/// and provides codec/BOM/mapping helpers as instance methods. Implicit
/// conversion from/to `Value` makes it interchangeable with plain enum
/// constants (`TextEncoding enc = TextEncoding::UTF8`).
class TextEncoding {
public:
    enum Value : int {
        UTF8,
        UTF8_BOM,
        UTF16_LE,
        UTF16_BE,
        Windows_1252,
        Windows_1250,
        Windows_1251,
        CP437,
        CP850,
        ISO_8859_1,
        System,
    };

    constexpr TextEncoding(const Value v) noexcept
    : m_encoding(v) {}

    constexpr operator Value() const noexcept { return m_encoding; }
    [[nodiscard]] constexpr auto value() const noexcept -> Value { return m_encoding; }

    friend constexpr auto operator==(const TextEncoding&, const TextEncoding&) noexcept -> bool = default;
    friend constexpr auto operator==(const TextEncoding lhs, const Value rhs) noexcept -> bool { return lhs.m_encoding == rhs; }

    /// All supported encoding values, stable order.
    static constexpr std::array<Value, 11> all {
        UTF8,
        UTF8_BOM,
        UTF16_LE,
        UTF16_BE,
        Windows_1252,
        Windows_1250,
        Windows_1251,
        CP437,
        CP850,
        ISO_8859_1,
        System,
    };

    // -----------------------------------------------------------------------
    // Stable config key (persisted to INI)
    // -----------------------------------------------------------------------

    [[nodiscard]] auto toString() const -> std::string_view;
    [[nodiscard]] static auto parse(std::string_view) -> std::optional<TextEncoding>;

    // -----------------------------------------------------------------------
    // wx mapping helpers
    // -----------------------------------------------------------------------

    [[nodiscard]] auto toWxBom() const -> wxBOM;
    [[nodiscard]] static auto fromWxBom(wxBOM) -> std::optional<TextEncoding>;

    // -----------------------------------------------------------------------
    // BOM bytes — wraps wxConvAuto::GetBOMChars
    // -----------------------------------------------------------------------

    [[nodiscard]] auto bomLength() const -> std::size_t;
    [[nodiscard]] auto bomBytes() const -> std::span<const std::byte>;

    // -----------------------------------------------------------------------
    // Codec — encode / decode payload bytes (BOM handled separately)
    // -----------------------------------------------------------------------

    [[nodiscard]] auto encode(const wxString& text) const -> std::optional<wxCharBuffer>;
    [[nodiscard]] auto decode(const void* bytes, std::size_t len) const -> std::optional<wxString>;

private:
    Value m_encoding;
};

/// Line-ending style. Maps onto wxSTC_EOL_* constants via toStc().
class EolMode {
public:
    enum Value : int {
        LF,
        CRLF,
        CR,
    };

    constexpr EolMode(const Value v) noexcept
    : m_mode(v) {}

    constexpr operator Value() const noexcept { return m_mode; }
    [[nodiscard]] constexpr auto value() const noexcept -> Value { return m_mode; }

    friend constexpr auto operator==(const EolMode&, const EolMode&) noexcept -> bool = default;
    friend constexpr auto operator==(const EolMode lhs, const Value rhs) noexcept -> bool { return lhs.m_mode == rhs; }

    static constexpr std::array<Value, 3> all { LF, CRLF, CR };

    [[nodiscard]] auto toString() const -> std::string_view;
    [[nodiscard]] static auto parse(std::string_view) -> std::optional<EolMode>;

    [[nodiscard]] auto toStc() const -> int;
    [[nodiscard]] static auto fromStc(int stcEolMode) -> EolMode;

private:
    Value m_mode;
};

} // namespace fbide
