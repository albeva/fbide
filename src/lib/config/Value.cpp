//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Value.hpp"
using namespace fbide;

namespace {

/// Walk `root` following dot-separated `path` and return a pointer to the
/// resolved node, or nullptr on any failure (missing key, non-table node,
/// empty segment).
auto resolve(toml::value& root, const std::string_view path) noexcept -> toml::value* {
    toml::value* cur = &root;
    std::size_t i = 0;

    while (i < path.size()) {
        const std::size_t keyStart = i;
        while (i < path.size() && path[i] != '.') {
            ++i;
        }
        if (i == keyStart) {
            return nullptr; // empty segment
        }
        if (!cur->is_table()) {
            return nullptr;
        }
        auto& table = cur->as_table();
        const auto it = table.find(std::string(path.substr(keyStart, i - keyStart)));
        if (it == table.end()) {
            return nullptr;
        }
        cur = &it->second;
        if (i < path.size()) {
            ++i; // skip '.'
        }
    }

    return cur;
}

} // anonymous namespace

// -------------------------------------------------------------------------
// Type checks
// -------------------------------------------------------------------------
auto Value::isString() const noexcept -> bool { return m_val->is_string(); }
auto Value::isInt() const noexcept -> bool    { return m_val->is_integer(); }
auto Value::isBool() const noexcept -> bool   { return m_val->is_boolean(); }
auto Value::isFloat() const noexcept -> bool  { return m_val->is_floating(); }
auto Value::isTable() const noexcept -> bool  { return m_val->is_table(); }
auto Value::isArray() const noexcept -> bool  { return m_val->is_array(); }

// -------------------------------------------------------------------------
// Direct reads
// -------------------------------------------------------------------------
auto Value::asString() const noexcept -> std::optional<wxString> {
    if (!m_val->is_string()) {
        return std::nullopt;
    }
    return wxString(m_val->as_string());
}

auto Value::asStdString() const noexcept -> std::optional<std::string> {
    if (!m_val->is_string()) {
        return std::nullopt;
    }
    return m_val->as_string();
}

auto Value::asInt() const noexcept -> std::optional<std::int64_t> {
    if (!m_val->is_integer()) {
        return std::nullopt;
    }
    return m_val->as_integer();
}

auto Value::asBool() const noexcept -> std::optional<bool> {
    if (!m_val->is_boolean()) {
        return std::nullopt;
    }
    return m_val->as_boolean();
}

auto Value::asFloat() const noexcept -> std::optional<double> {
    if (!m_val->is_floating()) {
        return std::nullopt;
    }
    return m_val->as_floating();
}

// -------------------------------------------------------------------------
// Path lookup
// -------------------------------------------------------------------------
auto Value::find(const wxString& path) const noexcept -> std::optional<Value> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr) {
        return std::nullopt;
    }
    return Value(*node);
}

// -------------------------------------------------------------------------
// Typed path reads
// -------------------------------------------------------------------------
auto Value::getString(const wxString& path) const noexcept -> std::optional<wxString> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr || !node->is_string()) {
        return std::nullopt;
    }
    return wxString(node->as_string());
}

auto Value::getStdString(const wxString& path) const noexcept -> std::optional<std::string> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr || !node->is_string()) {
        return std::nullopt;
    }
    return node->as_string();
}

auto Value::getInt(const wxString& path) const noexcept -> std::optional<std::int64_t> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr || !node->is_integer()) {
        return std::nullopt;
    }
    return node->as_integer();
}

auto Value::getBool(const wxString& path) const noexcept -> std::optional<bool> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr || !node->is_boolean()) {
        return std::nullopt;
    }
    return node->as_boolean();
}

auto Value::getFloat(const wxString& path) const noexcept -> std::optional<double> {
    auto* node = resolve(*m_val, path.ToStdString());
    if (node == nullptr || !node->is_floating()) {
        return std::nullopt;
    }
    return node->as_floating();
}

// -------------------------------------------------------------------------
// Typed path reads with default
// -------------------------------------------------------------------------
auto Value::getString(const wxString& path, const wxString& def) const noexcept -> wxString {
    return getString(path).value_or(def);
}

auto Value::getStdString(const wxString& path, std::string def) const noexcept -> std::string {
    return getStdString(path).value_or(std::move(def));
}

auto Value::getInt(const wxString& path, std::int64_t def) const noexcept -> std::int64_t {
    return getInt(path).value_or(def);
}

auto Value::getBool(const wxString& path, bool def) const noexcept -> bool {
    return getBool(path).value_or(def);
}

auto Value::getFloat(const wxString& path, double def) const noexcept -> double {
    return getFloat(path).value_or(def);
}
