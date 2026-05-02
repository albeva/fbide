//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * A configuration node.
 *
 * `Value` owns its subtree. Move-only — always capture by reference
 * from a `ConfigManager` accessor.
 *
 * State (`std::variant`):
 *   - `monostate` — invalid / empty
 *   - `wxString`  — leaf value
 *   - `Table`     — ordered children
 *
 * Path syntax: dot-separated keys, e.g. `"editor.tabSize"`. Empty
 * path = current node.
 *
 * @code{.cpp}
 * auto& cfg = ctx.getConfigManager().config();
 * const auto& editor = cfg.at("editor");
 * const auto tabSize = editor.get_or("tabSize", 4);
 * const auto name    = editor.get_or("title", "Editor");
 * @endcode
 *
 * Writes auto-create intermediate groups:
 *
 * @code{.cpp}
 * cfg["window"]["x"] = 100;
 * cfg["compiler"]["path"] = wxString{"fbc.exe"};
 * @endcode
 */
class Value final {
public:
    /// Child nodes. Unordered lookup — wxFileConfig handles output
    /// ordering on save. unique_ptr breaks the recursive type.
    using Table = std::unordered_map<wxString, std::unique_ptr<Value>>;

    /// Default-constructed invalid node (`monostate`).
    Value() = default;
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    /// Move-construct, leaving the source in a valid-but-empty state.
    Value(Value&&) noexcept = default;
    /// Move-assign, leaving the source in a valid-but-empty state.
    Value& operator=(Value&&) noexcept = default;

    /// True if this node holds a value (leaf or group), false for invalid.
    [[nodiscard]] explicit operator bool() const noexcept;

    // -------------------------------------------------------------------
    // Type probes
    // -------------------------------------------------------------------
    /// True when this node is a group of children.
    [[nodiscard]] auto isTable() const noexcept -> bool;
    /// True when this leaf parses as a string (always true for any leaf — sugar for `bool(*this) && !isTable()`).
    [[nodiscard]] auto isString() const noexcept -> bool;
    /// True when this leaf parses as an integer.
    [[nodiscard]] auto isInt() const noexcept -> bool;
    /// True when this leaf parses as a boolean (`true`/`false`/`yes`/`no`/`0`/`1`).
    [[nodiscard]] auto isBool() const noexcept -> bool;
    /// True when this leaf parses as a floating-point number.
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

    /// Read leaf as `bool`, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(bool def) const -> bool;
    /// Read leaf as `int`, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(int def) const -> int;
    /// Read leaf as `int64_t`, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(std::int64_t def) const -> std::int64_t;
    /// Read leaf as `double`, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(double def) const -> double;
    /// Read leaf as `wxString`, falling back to `def` on any failure.
    [[nodiscard]] auto value_or(const wxString& def) const -> wxString;

    /// Read leaf as `wxString` with a string-literal default — implicit cast.
    template<std::size_t N>
    [[nodiscard]] auto value_or(const char (&def)[N]) const -> wxString {
        return value_or(wxString { def, N > 0 ? N - 1 : 0 });
    }

    /// `at(path).value_or(def)` in one call.
    template<typename P, typename T>
    [[nodiscard]] auto get_or(const P& path, const T& def) const
        -> decltype(std::declval<const Value&>().value_or(def)) {
        return at(path).value_or(def);
    }

    /// `get_or` overload that accepts a string-literal default.
    template<typename P, std::size_t N>
    [[nodiscard]] auto get_or(const P& path, const char (&def)[N]) const -> wxString {
        return at(path).value_or(def);
    }

    /// Split a leaf wxString on `,` and return trimmed items. Returns
    /// empty vector if the node isn't a leaf.
    [[nodiscard]] auto asArray() const -> std::vector<wxString>;

    /// Group children in insertion order. Empty if not a group.
    [[nodiscard]] auto entries() const -> const Table&;

    // -------------------------------------------------------------------
    // Writes — replace this node's contents with a leaf
    // -------------------------------------------------------------------
    /// Assign a `bool` leaf.
    auto operator=(bool v) -> Value&;
    /// Assign an `int` leaf.
    auto operator=(int v) -> Value&;
    /// Assign an `int64_t` leaf.
    auto operator=(std::int64_t v) -> Value&;
    /// Assign a `double` leaf.
    auto operator=(double v) -> Value&;
    /// Assign a `wxString` leaf.
    auto operator=(const wxString& v) -> Value&;
    /// Assign a C-string leaf.
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

    /// Storage variant — invalid (`monostate`), leaf (`wxString`), or group (`Table`).
    std::variant<std::monostate, wxString, Table> m_data;
};

} // namespace fbide
