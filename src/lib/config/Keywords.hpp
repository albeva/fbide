//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Syntax keyword groups loaded from legacy .lng files.
/// 4 groups (matching Scintilla keyword sets) plus a sorted combined list.
class Keywords final {
public:
    static constexpr int GROUP_COUNT = 4;

    /// Load keywords from a legacy .lng file.
    void load(const wxString& filePath);

    /// Save keywords to a .lng file.
    void save() const;

    /// Get keyword group by index (0-3).
    [[nodiscard]] auto getGroup(const int index) const -> const wxString& { return m_groups[static_cast<size_t>(index)]; }

    /// Set keyword group by index (0-3).
    void setGroup(const int index, const wxString& keywords) { m_groups[static_cast<size_t>(index)] = keywords; }

    /// Get sorted combined word list (for autocomplete).
    [[nodiscard]] auto getSortedList() const -> const wxArrayString& { return m_sortedList; }

private:
    void buildSortedList();

    wxString m_langePath;
    std::array<wxString, GROUP_COUNT> m_groups {};
    wxArrayString m_sortedList;
};

} // namespace fbide
