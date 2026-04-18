//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Non-owning cursor into a toml value tree.
///
/// Cheap to copy (holds a single pointer). Invalid (null) Value may result
/// from a missing path — all read methods handle this gracefully.
///
/// Path syntax: dot-separated keys, e.g. "editor.tabSize". Empty path =
/// current node.
///
/// Typical usage:
///
///     auto cfg = ctx.getConfigManager().config();          // root
///     auto editor = cfg.at("editor");                      // cached section
///     const auto tabSize    = editor.get_or("tabSize", 4);
///     const auto lineNumber = editor.get_or("lineNumbers", true);
///
/// Writes auto-create intermediate tables:
///
///     cfg["window"]["x"]      = 100;
///     cfg["compiler"]["path"] = wxString{"fbc.exe"};
class Value final {
public:
    using Inner = toml::basic_value<toml::ordered_type_config>;

    Value() = default;

    explicit Value(Inner& v) noexcept
    : m_val(&v) {}

    explicit Value(Inner* v) noexcept
    : m_val(v) {}

    /// Is this Value pointing at a real toml node?
    [[nodiscard]] explicit operator bool() const noexcept { return m_val != nullptr; }

    /// Underlying toml node. Precondition: isValid().
    [[nodiscard]] auto raw() const noexcept -> Inner& { return *m_val; }

    // -------------------------------------------------------------------
    // Type checks
    // -------------------------------------------------------------------
    [[nodiscard]] auto isTable() const noexcept -> bool;
    [[nodiscard]] auto isArray() const noexcept -> bool;
    [[nodiscard]] auto isString() const noexcept -> bool;
    [[nodiscard]] auto isInt() const noexcept -> bool;
    [[nodiscard]] auto isBool() const noexcept -> bool;
    [[nodiscard]] auto isFloat() const noexcept -> bool;

    // -------------------------------------------------------------------
    // Navigate (read) — returns an invalid Value if the path can't be
    // resolved. Never creates.
    // -------------------------------------------------------------------

    [[nodiscard]] auto at(std::string_view path) const -> Value;

    template<typename W>
        requires std::same_as<std::remove_cvref_t<W>, wxString>
    [[nodiscard]] auto at(const W& path) const -> Value {
        return at(asView(path));
    }

    // -------------------------------------------------------------------
    // Navigate (write) — auto-creates intermediate tables. Precondition:
    // this Value is valid.
    // -------------------------------------------------------------------

    [[nodiscard]] auto operator[](std::string_view path) -> Value;

    template<typename W>
        requires std::same_as<std::remove_cvref_t<W>, wxString>
    [[nodiscard]] auto operator[](const W& path) -> Value {
        return (*this)[asView(path)];
    }

    // -------------------------------------------------------------------
    // Typed read of the current node — returns nullopt if missing or
    // wrong type.
    // -------------------------------------------------------------------

    template<typename T>
    [[nodiscard]] auto as() const -> std::optional<T>;

    // -------------------------------------------------------------------
    // Read current node with default. Default type drives the return type.
    // -------------------------------------------------------------------

    [[nodiscard]] auto value_or(bool def) const -> bool;
    [[nodiscard]] auto value_or(int def) const -> int;
    [[nodiscard]] auto value_or(std::int64_t def) const -> std::int64_t;
    [[nodiscard]] auto value_or(double def) const -> double;
    [[nodiscard]] auto value_or(const wxString& def) const -> wxString;
    [[nodiscard]] auto value_or(const std::string& def) const -> std::string;

    template<std::size_t N>
    [[nodiscard]] auto value_or(const char (&def)[N]) const -> wxString {
        return value_or(wxString { def });
    }

    // -------------------------------------------------------------------
    // Combined navigate + read. Invalid path falls back to default.
    // -------------------------------------------------------------------

    template<typename P, typename T>
    [[nodiscard]] auto get_or(const P& path, const T& def) const
        -> decltype(std::declval<const Value&>().value_or(def)) {
        return at(path).value_or(def);
    }

    template<typename P, std::size_t N>
    [[nodiscard]] auto get_or(const P& path, const char (&def)[N]) const -> wxString {
        return at(path).value_or(def);
    }

    // -------------------------------------------------------------------
    // Write — assignment to current node. Requires valid Value.
    // -------------------------------------------------------------------

    auto operator=(bool v) -> Value&;
    auto operator=(int v) -> Value&;
    auto operator=(std::int64_t v) -> Value&;
    auto operator=(double v) -> Value&;
    auto operator=(const wxString& v) -> Value&;
    auto operator=(const std::string& v) -> Value&;
    auto operator=(const char* v) -> Value&;

private:
    static auto asView(const wxString& s) -> std::string;

    Inner* m_val = nullptr;
};

} // namespace fbide
