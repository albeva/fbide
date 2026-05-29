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

auto Value::clone() const -> Value {
    Value out;
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        out.m_data = *leaf;
    } else if (const auto* group = std::get_if<Table>(&m_data)) {
        Table copied;
        copied.reserve(group->size());
        for (const auto& [key, child] : *group) {
            copied.emplace(key, std::make_unique<Value>(child->clone()));
        }
        out.m_data = std::move(copied);
    }
    return out;
}

auto Value::erase(const wxString& path) -> bool {
    if (path.empty()) {
        return false;
    }
    // Walk to the parent of the leaf key.
    auto* cur = this;
    std::size_t start = 0;
    wxString leafKey;
    while (start <= path.length()) {
        const auto dot = path.find(PATH_SEP, start);
        const auto end = (dot == wxString::npos) ? path.length() : dot;
        if (end == start) {
            return false;
        }
        const auto segment = path.SubString(start, end - 1);
        if (dot == wxString::npos) {
            leafKey = segment;
            break;
        }
        auto* group = std::get_if<Table>(&cur->m_data);
        if (group == nullptr) {
            return false;
        }
        const auto it = group->find(segment);
        if (it == group->end()) {
            return false;
        }
        cur = it->second.get();
        start = dot + 1;
    }
    auto* group = std::get_if<Table>(&cur->m_data);
    if (group == nullptr) {
        return false;
    }
    return group->erase(leafKey) > 0;
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
/// Parse leaf as bool — accepts `true`/`false`/`yes`/`no`/`0`/`1`.
template<>
auto Value::as<bool>() const -> std::optional<bool> {
    if (const auto* leaf = std::get_if<wxString>(&m_data)) {
        return parseBool(*leaf);
    }
    return std::nullopt;
}

/// Parse leaf as int via `wxString::ToLong`.
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

/// Parse leaf as int64_t via `wxString::ToLongLong`.
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

/// Parse leaf as double via `wxString::ToDouble`.
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

/// Pull the raw `wxString` leaf value through unchanged.
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

// -------------------------------------------------------------------------
// Overlay merge
// -------------------------------------------------------------------------
void Value::mergeFrom(const Value& other) {
    // Invalid overlay = no-op. Lets callers pass a default-constructed
    // Value when the overlay file is absent without checking first.
    if (!other) {
        return;
    }

    // Overlay leaf — replace this whole subtree. Captures both the
    // same-leaf case and the type-mismatch case (overlay leaf where
    // we're a group). `operator=(const wxString&)` resets m_data to a
    // leaf, dropping any prior Table.
    if (!other.isTable()) {
        *this = other.as<wxString>().value_or(wxString {});
        return;
    }

    // Overlay group — recurse per child. `operator[]` converts this node
    // to a Table if it isn't already (type-mismatch: overlay group where
    // we're a leaf), and creates missing children on demand.
    for (const auto& [key, child] : other.entries()) {
        (*this)[key].mergeFrom(*child);
    }
}

auto Value::diffAgainst(const Value& baseline) const -> Value {
    Value out;

    // Leaf (or invalid) at this node — emit only if our leaf actually
    // differs from baseline's leaf at the same point. Type mismatch
    // (baseline is a group, or baseline is absent) counts as divergence
    // and forces the leaf out.
    if (!isTable()) {
        if (!*this) {
            return out;
        }
        const auto mergedLeaf = as<wxString>().value_or(wxString {});
        if (baseline.isTable() || !baseline) {
            out = mergedLeaf;
            return out;
        }
        const auto baselineLeaf = baseline.as<wxString>().value_or(wxString {});
        if (mergedLeaf != baselineLeaf) {
            out = mergedLeaf;
        }
        return out;
    }

    // Group — recurse per child. Children present only in baseline are
    // never visited, so deletion is intentionally unexpressible by diff.
    // Children whose own diff is empty are skipped, so parent groups are
    // synthesised on `out` only when they contain a divergence.
    for (const auto& [key, child] : entries()) {
        Value childDiff = child->diffAgainst(baseline.at(key));
        if (childDiff) {
            // operator[] creates the child slot under out; move-assign
            // embeds the recursive diff via the defaulted Value&& operator.
            out[key] = std::move(childDiff);
        }
    }
    return out;
}
