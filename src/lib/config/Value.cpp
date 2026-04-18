//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Value.hpp"
using namespace fbide;

namespace {
constexpr auto DOT = '.';
} // namespace

auto Value::asView(const wxString& s) -> std::string {
    return s.ToStdString();
}

// -------------------------------------------------------------------------
// Type checks
// -------------------------------------------------------------------------
auto Value::isTable() const noexcept -> bool {
    return m_val != nullptr && m_val->is_table();
}

auto Value::isArray() const noexcept -> bool {
    return m_val != nullptr && m_val->is_array();
}

auto Value::isString() const noexcept -> bool {
    return m_val != nullptr && m_val->is_string();
}

auto Value::isInt() const noexcept -> bool {
    return m_val != nullptr && m_val->is_integer();
}

auto Value::isBool() const noexcept -> bool {
    return m_val != nullptr && m_val->is_boolean();
}

auto Value::isFloat() const noexcept -> bool {
    return m_val != nullptr && m_val->is_floating();
}

// -------------------------------------------------------------------------
// Navigation (read)
// -------------------------------------------------------------------------
auto Value::at(const std::string_view path) const -> Value {
    if (m_val == nullptr) {
        return Value {};
    }
    if (path.empty()) {
        return *this;
    }
    auto* cur = m_val;
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto dot = path.find(DOT, start);
        const auto end = (dot == std::string_view::npos) ? path.size() : dot;
        const auto seg = path.substr(start, end - start);
        if (seg.empty()) {
            return Value {};
        }
        if (!cur->is_table()) {
            return Value {};
        }
        auto& table = cur->as_table();
        const auto it = table.find(std::string { seg });
        if (it == table.end()) {
            return Value {};
        }
        cur = &it->second;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return Value { *cur };
}

// -------------------------------------------------------------------------
// Navigation (write) — auto-creates intermediate tables
// -------------------------------------------------------------------------
auto Value::operator[](const std::string_view path) -> Value {
    if (m_val == nullptr) {
        return Value {};
    }
    if (path.empty()) {
        return *this;
    }
    auto* cur = m_val;
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto dot = path.find(DOT, start);
        const auto end = (dot == std::string_view::npos) ? path.size() : dot;
        const auto seg = path.substr(start, end - start);
        if (seg.empty()) {
            return Value {};
        }
        if (!cur->is_table()) {
            *cur = Inner { Inner::table_type {} };
        }
        auto& table = cur->as_table();
        cur = &table[std::string { seg }];
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return Value { *cur };
}

// -------------------------------------------------------------------------
// Typed read (optional)
// -------------------------------------------------------------------------
template<>
auto Value::as<bool>() const -> std::optional<bool> {
    return isBool() ? std::optional { m_val->as_boolean() } : std::nullopt;
}

template<>
auto Value::as<int>() const -> std::optional<int> {
    return isInt() ? std::optional { static_cast<int>(m_val->as_integer()) } : std::nullopt;
}

template<>
auto Value::as<std::int64_t>() const -> std::optional<std::int64_t> {
    return isInt() ? std::optional { m_val->as_integer() } : std::nullopt;
}

template<>
auto Value::as<double>() const -> std::optional<double> {
    return isFloat() ? std::optional { m_val->as_floating() } : std::nullopt;
}

template<>
auto Value::as<std::string>() const -> std::optional<std::string> {
    return isString() ? std::optional { m_val->as_string() } : std::nullopt;
}

template<>
auto Value::as<wxString>() const -> std::optional<wxString> {
    if (!isString()) {
        return std::nullopt;
    }
    const auto& raw = m_val->as_string();
    return wxString::FromUTF8(raw.data(), raw.size());
}

auto Value::asArray() const -> std::vector<Value> {
    std::vector<Value> out;
    if (!isArray()) {
        return out;
    }
    auto& arr = m_val->as_array();
    out.reserve(arr.size());
    for (auto& item : arr) {
        out.emplace_back(item);
    }
    return out;
}

// -------------------------------------------------------------------------
// Read with default
// -------------------------------------------------------------------------
auto Value::value_or(const bool def) const -> bool {
    return as<bool>().value_or(def);
}

auto Value::value_or(const int def) const -> int {
    return as<int>().value_or(def);
}

auto Value::value_or(const std::int64_t def) const -> std::int64_t {
    return as<std::int64_t>().value_or(def);
}

auto Value::value_or(const double def) const -> double {
    return as<double>().value_or(def);
}

auto Value::value_or(const wxString& def) const -> wxString {
    return as<wxString>().value_or(def);
}

auto Value::value_or(const std::string& def) const -> std::string {
    return as<std::string>().value_or(def);
}

// -------------------------------------------------------------------------
// Write (precondition: m_val is valid)
// -------------------------------------------------------------------------
auto Value::operator=(const bool v) -> Value& {
    *m_val = v;
    return *this;
}

auto Value::operator=(const int v) -> Value& {
    *m_val = static_cast<std::int64_t>(v);
    return *this;
}

auto Value::operator=(const std::int64_t v) -> Value& {
    *m_val = v;
    return *this;
}

auto Value::operator=(const double v) -> Value& {
    *m_val = v;
    return *this;
}

auto Value::operator=(const wxString& v) -> Value& {
    const auto utf8 = v.utf8_str();
    *m_val = std::string { utf8.data(), utf8.length() };
    return *this;
}

auto Value::operator=(const std::string& v) -> Value& {
    *m_val = v;
    return *this;
}

auto Value::operator=(const char* v) -> Value& {
    *m_val = std::string { v };
    return *this;
}
