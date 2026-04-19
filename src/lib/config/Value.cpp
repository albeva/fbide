//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Value.hpp"
using namespace fbide;

namespace {
constexpr auto PATH_SEP = '.';
constexpr auto ARRAY_SEP = ',';
} // namespace

// -------------------------------------------------------------------------
// Boolean-ish coercion
// -------------------------------------------------------------------------
namespace {
auto parseBool(const wxString& s) -> std::optional<bool> {
    if (s == "1") {
        return true;
    }
    if (s == "0") {
        return false;
    }
    if (s.CmpNoCase("true") == 0 || s.CmpNoCase("yes") == 0) {
        return true;
    }
    if (s.CmpNoCase("false") == 0 || s.CmpNoCase("no") == 0) {
        return false;
    }
    return std::nullopt;
}
} // namespace

// -------------------------------------------------------------------------
// Basic state
// -------------------------------------------------------------------------
Value::operator bool() const noexcept {
    return !std::holds_alternative<std::monostate>(m_data);
}

auto Value::isTable() const noexcept -> bool {
    return std::holds_alternative<Table>(m_data);
}

auto Value::isString() const noexcept -> bool {
    // Any leaf is a string by storage; callers distinguish typed content
    // via `isInt`/`isBool`/`isFloat` (which also return true).
    return std::holds_alternative<wxString>(m_data);
}

auto Value::isInt() const noexcept -> bool {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        long tmp = 0;
        return leaf->ToLong(&tmp);
    }
    return false;
}

auto Value::isBool() const noexcept -> bool {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        return parseBool(*leaf).has_value();
    }
    return false;
}

auto Value::isFloat() const noexcept -> bool {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        double tmp = 0;
        return leaf->ToDouble(&tmp);
    }
    return false;
}

// -------------------------------------------------------------------------
// Sentinel — returned by at() on miss. Valid for program lifetime.
// -------------------------------------------------------------------------
auto Value::invalidValue() -> const Value& {
    static const Value sentinel {};
    return sentinel;
}

// -------------------------------------------------------------------------
// Navigation
// -------------------------------------------------------------------------
auto Value::findChild(const wxString& key) const -> const Value* {
    const auto* group = std::get_if<Table>(&m_data);
    if (group == nullptr) {
        return nullptr;
    }
    const auto it = group->find(key);
    return it != group->end() ? it->second.get() : nullptr;
}

auto Value::findOrCreateChild(const wxString& key) -> Value* {
    if (!std::holds_alternative<Table>(m_data)) {
        m_data = Table {};
    }
    auto& group = std::get<Table>(m_data);
    auto [it, inserted] = group.try_emplace(key, std::make_unique<Value>());
    return it->second.get();
}

auto Value::at(const wxString& path) const -> const Value& {
    if (path.empty()) {
        return *this;
    }
    auto cur = this;
    std::size_t start = 0;
    while (start <= path.length()) {
        const auto dot = path.find(PATH_SEP, start);
        const auto end = (dot == wxString::npos) ? path.length() : dot;
        if (end == start) {
            return invalidValue();
        }
        const auto key = path.SubString(start, end - 1);
        const auto* next = cur->findChild(key);
        if (next == nullptr) {
            return invalidValue();
        }
        cur = next;
        if (dot == wxString::npos) {
            break;
        }
        start = dot + 1;
    }
    return *cur;
}

auto Value::operator[](const wxString& path) -> Value& {
    if (path.empty()) {
        return *this;
    }
    auto cur = this;
    std::size_t start = 0;
    while (start <= path.length()) {
        const auto dot = path.find(PATH_SEP, start);
        const auto end = (dot == wxString::npos) ? path.length() : dot;
        if (end == start) {
            break;
        }
        const auto key = path.SubString(start, end - 1);
        cur = cur->findOrCreateChild(key);
        if (dot == wxString::npos) {
            break;
        }
        start = dot + 1;
    }
    return *cur;
}

// -------------------------------------------------------------------------
// Typed reads
// -------------------------------------------------------------------------
template<>
auto Value::as<bool>() const -> std::optional<bool> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        return parseBool(*leaf);
    }
    return std::nullopt;
}

template<>
auto Value::as<int>() const -> std::optional<int> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        long tmp = 0;
        if (leaf->ToLong(&tmp)) {
            return static_cast<int>(tmp);
        }
    }
    return std::nullopt;
}

template<>
auto Value::as<std::int64_t>() const -> std::optional<std::int64_t> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        wxLongLong_t tmp = 0;
        if (leaf->ToLongLong(&tmp)) {
            return static_cast<std::int64_t>(tmp);
        }
    }
    return std::nullopt;
}

template<>
auto Value::as<double>() const -> std::optional<double> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        double tmp = 0;
        if (leaf->ToDouble(&tmp)) {
            return tmp;
        }
    }
    return std::nullopt;
}

template<>
auto Value::as<wxString>() const -> std::optional<wxString> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        return *leaf;
    }
    return std::nullopt;
}

// -------------------------------------------------------------------------
// value_or wrappers
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

// -------------------------------------------------------------------------
// Arrays (leaf string split on ',')
// -------------------------------------------------------------------------
auto Value::asArray() const -> std::vector<wxString> {
    std::vector<wxString> out;
    const auto* leaf = std::get_if<wxString>(&m_data);
    if (leaf == nullptr || leaf->empty()) {
        return out;
    }
    std::size_t start = 0;
    while (start <= leaf->length()) {
        const auto sep = leaf->find(ARRAY_SEP, start);
        const auto end = (sep == wxString::npos) ? leaf->length() : sep;
        auto item = leaf->SubString(start, end - 1);
        item.Trim(true).Trim(false);
        if (not item.empty()) {
            out.emplace_back(std::move(item));
        }
        if (sep == wxString::npos) {
            break;
        }
        start = sep + 1;
    }
    return out;
}

auto Value::entries() const -> const Table& {
    static const Table empty {};
    const auto* group = std::get_if<Table>(&m_data);
    return group != nullptr ? *group : empty;
}

// -------------------------------------------------------------------------
// Writes
// -------------------------------------------------------------------------
auto Value::operator=(const bool v) -> Value& {
    m_data = wxString { v ? "1" : "0" };
    return *this;
}

auto Value::operator=(const int v) -> Value& {
    m_data = wxString::Format("%d", v);
    return *this;
}

auto Value::operator=(const std::int64_t v) -> Value& {
    m_data = wxString::Format("%lld", static_cast<long long>(v));
    return *this;
}

auto Value::operator=(const double v) -> Value& {
    m_data = wxString::FromDouble(v);
    return *this;
}

auto Value::operator=(const wxString& v) -> Value& {
    m_data = v;
    return *this;
}

auto Value::operator=(const char* v) -> Value& {
    m_data = wxString { v };
    return *this;
}
