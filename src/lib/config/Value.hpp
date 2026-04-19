//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// A configuration node.
///
/// Value owns its subtree. Move-only — always capture by reference from
/// a `ConfigManager` accessor.
///
/// State (std::variant):
///   - monostate   — invalid / empty
///   - wxString    — leaf value
///   - Group       — ordered children
///
/// Path syntax: dot-separated keys, e.g. "editor.tabSize". Empty path =
/// current node.
///
/// Typical usage:
///
///     auto& cfg = ctx.getConfigManager().config();
///     const auto& editor = cfg.at("editor");
///     const auto tabSize = editor.get_or("tabSize", 4);
///     const auto name    = editor.get_or("title", "Editor");
///
/// Writes auto-create intermediate groups:
///
///     cfg["window"]["x"] = 100;
///     cfg["compiler"]["path"] = wxString{"fbc.exe"};
class Value final {
public:
    /// Ordered child nodes. Vector preserves insertion order; unique_ptr
    /// breaks the recursive type.
    using Group = std::vector<std::pair<wxString, std::unique_ptr<Value>>>;

    Value() = default;
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&&) noexcept = default;
    Value& operator=(Value&&) noexcept = default;

    /// True if this node holds a value (leaf or group), false for invalid.
    [[nodiscard]] explicit operator bool() const noexcept;

    // -------------------------------------------------------------------
    // Type probes
    // -------------------------------------------------------------------
    [[nodiscard]] auto isTable() const noexcept -> bool;
    [[nodiscard]] auto isString() const noexcept -> bool;
    [[nodiscard]] auto isInt() const noexcept -> bool;
    [[nodiscard]] auto isBool() const noexcept -> bool;
    [[nodiscard]] auto isFloat() const noexcept -> bool;

    // -------------------------------------------------------------------
    // Navigation
    // -------------------------------------------------------------------

    /// Walk dot-separated path through groups. Returns reference to
    /// `invalidValue()` sentinel if any segment is missing or current
    /// node is a leaf.
    [[nodiscard]] auto at(const wxString& path) const -> const Value&;

    /// Write-side navigation. Creates intermediate groups as needed.
    /// Reference is valid until any sibling creation resizes this group's
    /// vector — use the returned Value& immediately (assign or chain).
    [[nodiscard]] auto operator[](const wxString& path) -> Value&;

    // -------------------------------------------------------------------
    // Typed reads
    // -------------------------------------------------------------------

    /// Parse leaf value as T; returns nullopt if not a leaf or parse fails.
    template<typename T>
    [[nodiscard]] auto as() const -> std::optional<T>;

    /// Read leaf as T, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(bool def) const -> bool;
    [[nodiscard]] auto value_or(int def) const -> int;
    [[nodiscard]] auto value_or(std::int64_t def) const -> std::int64_t;
    [[nodiscard]] auto value_or(double def) const -> double;
    [[nodiscard]] auto value_or(const wxString& def) const -> wxString;
    template<std::size_t N>
    [[nodiscard]] auto value_or(const char (&def)[N]) const -> wxString {
        return value_or(wxString { def });
    }

    /// `at(path).value_or(def)` in one call.
    template<typename P, typename T>
    [[nodiscard]] auto get_or(const P& path, const T& def) const
        -> decltype(std::declval<const Value&>().value_or(def)) {
        return at(path).value_or(def);
    }
    template<typename P, std::size_t N>
    [[nodiscard]] auto get_or(const P& path, const char (&def)[N]) const -> wxString {
        return at(path).value_or(def);
    }

    /// Split a leaf wxString on `,` and return trimmed items. Returns
    /// empty vector if the node isn't a leaf.
    [[nodiscard]] auto asArray() const -> std::vector<wxString>;

    /// Group children in insertion order. Empty if not a group.
    [[nodiscard]] auto entries() const -> const Group&;

    // -------------------------------------------------------------------
    // Writes — replace this node's contents with a leaf
    // -------------------------------------------------------------------
    auto operator=(bool v) -> Value&;
    auto operator=(int v) -> Value&;
    auto operator=(std::int64_t v) -> Value&;
    auto operator=(double v) -> Value&;
    auto operator=(const wxString& v) -> Value&;
    auto operator=(const char* v) -> Value&;

    /// Shared invalid sentinel returned by `at()` on a miss.
    [[nodiscard]] static auto invalidValue() -> const Value&;

private:
    /// Look up a direct child by key (no path splitting). Returns nullptr
    /// if not a group or the key is not present.
    [[nodiscard]] auto findChild(const wxString& key) const -> const Value*;

    /// Get or create a direct child by key. If current node isn't a
    /// group, it is converted to one (losing any existing leaf value).
    [[nodiscard]] auto findOrCreateChild(const wxString& key) -> Value*;

    std::variant<std::monostate, wxString, Group> m_data;
};

} // namespace fbide
