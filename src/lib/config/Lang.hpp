//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "LangId.hpp"

namespace fbide {

/// Manages UI translation strings.
/// Loads legacy .fbl files (INI format with numeric keys under [FBIde] section).
class Lang final {
public:
    NO_COPY_AND_MOVE(Lang)
    Lang() = default;

    /// Load translations from a legacy .fbl file.
    /// Clears any previously loaded strings before loading.
    void load(const wxString& filePath);

    /// Clear all loaded translations.
    void clear();

    /// Get translation string by id. Returns empty string if not found.
    [[nodiscard]] auto get(LangId id) const -> const wxString&;

    /// Shorthand for get().
    [[nodiscard]] auto operator[](const LangId id) const -> const wxString& { return get(id); }

private:
    static constexpr int maxId = 250;
    std::vector<wxString> m_strings;
};

} // namespace fbide
