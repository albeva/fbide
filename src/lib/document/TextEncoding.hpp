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

/**
 * Text file encodings supported by the editor.
 *
 * Thin wrapper around the nested `Value` enum — holds a single
 * `m_encoding` and provides codec / BOM / mapping helpers as
 * instance methods. Implicit conversion from/to `Value` makes it
 * interchangeable with plain enum constants
 * (`TextEncoding enc = TextEncoding::UTF8`).
 */
class TextEncoding {
public:
    /// Stable identifier for each supported encoding.
    enum Value : int {
        UTF8,         ///< UTF-8 without BOM.
        UTF8_BOM,     ///< UTF-8 with BOM.
        UTF16_LE,     ///< UTF-16 little-endian (with BOM).
        UTF16_BE,     ///< UTF-16 big-endian (with BOM).
        Windows_1252, ///< Western European Windows code page.
        Windows_1250, ///< Central European Windows code page.
        Windows_1251, ///< Cyrillic Windows code page.
        CP437,        ///< Original IBM PC code page.
        CP850,        ///< Western European DOS code page.
        ISO_8859_1,   ///< Latin-1.
        System,       ///< Platform default (`wxConvLocal`).
    };

    /// Implicit construction from the underlying enum value.
    constexpr TextEncoding(const Value v) noexcept
    : m_encoding(v) {}

    /// Implicit conversion back to the underlying enum value.
    constexpr operator Value() const noexcept { return m_encoding; }
    /// Explicit access to the underlying enum value.
    [[nodiscard]] constexpr auto value() const noexcept -> Value { return m_encoding; }

    /// Defaulted equality across two `TextEncoding` instances.
    friend constexpr auto operator==(const TextEncoding&, const TextEncoding&) noexcept -> bool = default;
    /// Equality against a raw enum value.
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

    /// Stable INI key for this encoding.
    [[nodiscard]] auto toString() const -> std::string_view;
    /// Parse a stable INI key. Returns `nullopt` for unknown input.
    [[nodiscard]] static auto parse(std::string_view) -> std::optional<TextEncoding>;

    // -----------------------------------------------------------------------
    // wx mapping helpers
    // -----------------------------------------------------------------------

    /// Convert to the matching `wxBOM` enum value.
    [[nodiscard]] auto toWxBom() const -> wxBOM;
    /// Reverse mapping from `wxBOM`. Returns `nullopt` for unmapped values.
    [[nodiscard]] static auto fromWxBom(wxBOM) -> std::optional<TextEncoding>;

    // -----------------------------------------------------------------------
    // BOM bytes — wraps wxConvAuto::GetBOMChars
    // -----------------------------------------------------------------------

    /// BOM length in bytes (0 when this encoding has no BOM).
    [[nodiscard]] auto bomLength() const -> std::size_t;
    /// BOM byte sequence (empty span when this encoding has no BOM).
    [[nodiscard]] auto bomBytes() const -> std::span<const std::byte>;

    // -----------------------------------------------------------------------
    // Codec — encode / decode payload bytes (BOM handled separately)
    // -----------------------------------------------------------------------

    /// Encode `text` with this encoding. Returns `nullopt` if the codec
    /// rejects characters in the input.
    [[nodiscard]] auto encode(const wxString& text) const -> std::optional<wxCharBuffer>;
    /// Decode `len` bytes at `bytes` with this encoding. Returns `nullopt`
    /// when the codec rejects the input.
    [[nodiscard]] auto decode(const void* bytes, std::size_t len) const -> std::optional<wxString>;

private:
    Value m_encoding; ///< Underlying enum value.
};

/// Line-ending style. Maps onto `wxSTC_EOL_*` constants via `toStc()`.
class EolMode {
public:
    /// Stable identifier for each supported EOL style.
    enum Value : int {
        LF,   ///< Unix-style `\n`.
        CRLF, ///< Windows-style `\r\n`.
        CR,   ///< Classic Mac-style `\r`.
    };

    /// Implicit construction from the underlying enum value.
    constexpr EolMode(const Value v) noexcept
    : m_mode(v) {}

    /// Implicit conversion back to the underlying enum value.
    constexpr operator Value() const noexcept { return m_mode; }
    /// Explicit access to the underlying enum value.
    [[nodiscard]] constexpr auto value() const noexcept -> Value { return m_mode; }

    /// Defaulted equality across two `EolMode` instances.
    friend constexpr auto operator==(const EolMode&, const EolMode&) noexcept -> bool = default;
    /// Equality against a raw enum value.
    friend constexpr auto operator==(const EolMode lhs, const Value rhs) noexcept -> bool { return lhs.m_mode == rhs; }

    /// All supported EOL values, stable order.
    static constexpr std::array<Value, 3> all { LF, CRLF, CR };

    /// Stable INI key for this EOL mode.
    [[nodiscard]] auto toString() const -> std::string_view;
    /// Parse a stable INI key. Returns `nullopt` for unknown input.
    [[nodiscard]] static auto parse(std::string_view) -> std::optional<EolMode>;

    /// Convert to the matching `wxSTC_EOL_*` constant.
    [[nodiscard]] auto toStc() const -> int;
    /// Reverse mapping from a `wxSTC_EOL_*` constant.
    [[nodiscard]] static auto fromStc(int stcEolMode) -> EolMode;

private:
    Value m_mode; ///< Underlying enum value.
};

} // namespace fbide
