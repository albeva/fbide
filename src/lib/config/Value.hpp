//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Non-owning view over a toml::value.
///
/// Provides ergonomic typed and path-based access. All getters return
/// std::optional — any failure (missing key, wrong type, malformed path)
/// silently yields std::nullopt.
///
/// Path syntax: dot-separated table keys, e.g. "dialogs.format.title".
///
/// Strings default to wxString. Use asStdString / getStdString when
/// working directly with toml11 / UTF-8 raw strings.
///
/// Note: the wrapped toml::value is non-const, so mutation is possible
/// via raw(). All getXxx / asXxx methods are read-only regardless.
class Value final {
public:
    explicit Value(toml::value& v) noexcept
    : m_val(&v) {}

    // -------------------------------------------------------------------
    // Type checks on the current node
    // -------------------------------------------------------------------
    [[nodiscard]] auto isString() const noexcept -> bool;
    [[nodiscard]] auto isInt() const noexcept -> bool;
    [[nodiscard]] auto isBool() const noexcept -> bool;
    [[nodiscard]] auto isFloat() const noexcept -> bool;
    [[nodiscard]] auto isTable() const noexcept -> bool;
    [[nodiscard]] auto isArray() const noexcept -> bool;

    // -------------------------------------------------------------------
    // Direct typed reads (no path) on the current node
    // -------------------------------------------------------------------
    [[nodiscard]] auto asString() const noexcept -> std::optional<wxString>;
    [[nodiscard]] auto asStdString() const noexcept -> std::optional<std::string>;
    [[nodiscard]] auto asInt() const noexcept -> std::optional<std::int64_t>;
    [[nodiscard]] auto asBool() const noexcept -> std::optional<bool>;
    [[nodiscard]] auto asFloat() const noexcept -> std::optional<double>;

    // -------------------------------------------------------------------
    // Path lookup — returns a view of the sub-node, or nullopt if the
    // path cannot be resolved.
    // -------------------------------------------------------------------
    [[nodiscard]] auto find(const wxString& path) const noexcept -> std::optional<Value>;

    // -------------------------------------------------------------------
    // Typed path reads — combine find() + asXxx()
    // -------------------------------------------------------------------
    [[nodiscard]] auto getString(const wxString& path) const noexcept -> std::optional<wxString>;
    [[nodiscard]] auto getStdString(const wxString& path) const noexcept -> std::optional<std::string>;
    [[nodiscard]] auto getInt(const wxString& path) const noexcept -> std::optional<std::int64_t>;
    [[nodiscard]] auto getBool(const wxString& path) const noexcept -> std::optional<bool>;
    [[nodiscard]] auto getFloat(const wxString& path) const noexcept -> std::optional<double>;

    // -------------------------------------------------------------------
    // Typed path reads with fallback default
    // -------------------------------------------------------------------
    [[nodiscard]] auto getString(const wxString& path, const wxString& def) const noexcept -> wxString;
    [[nodiscard]] auto getStdString(const wxString& path, std::string def) const noexcept -> std::string;
    [[nodiscard]] auto getInt(const wxString& path, std::int64_t def) const noexcept -> std::int64_t;
    [[nodiscard]] auto getBool(const wxString& path, bool def) const noexcept -> bool;
    [[nodiscard]] auto getFloat(const wxString& path, double def) const noexcept -> double;

    // -------------------------------------------------------------------
    // Escape hatch — direct access to the underlying toml11 value.
    // -------------------------------------------------------------------
    [[nodiscard]] auto raw() const noexcept -> toml::value& { return *m_val; }

private:
    toml::value* m_val;
};

} // namespace fbide
