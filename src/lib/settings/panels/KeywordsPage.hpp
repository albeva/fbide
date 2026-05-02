//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "config/ThemeCategory.hpp"
#include "format/transformers/case/CaseTransform.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {

/// Keywords settings tab — keyword group editor with per-group case dropdown.
class KeywordsPage final : public Panel {
public:
    NO_COPY_AND_MOVE(KeywordsPage)

    /// Construct without populating widgets; `create()` builds the UI.
    explicit KeywordsPage(Context& ctx, wxWindow* parent);
    /// Build the panel widgets.
    void create() override;
    /// Commit edits back into `ConfigManager`.
    void apply() override;

private:
    /// Locale lookup with empty default — sugar over `ConfigManager::locale().get_or`.
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    /// Group dropdown changed — stash current edits then load the new group.
    void onGroupChanged(const wxCommandEvent& event);
    /// Case-mode dropdown changed — record the new mode for the current group.
    void onCaseChanged(const wxCommandEvent& event);
    /// Save the textbox contents back into `m_groups[m_selectedGroup]`.
    void stashCurrent();

    Unowned<wxChoice> m_groupChoice;     ///< Keyword-group dropdown.
    Unowned<wxChoice> m_caseChoice;      ///< Case-mode dropdown.
    Unowned<wxTextCtrl> m_textKeywords;  ///< Multi-line keyword editor.
    std::size_t m_selectedGroup = 0;     ///< Index of the currently displayed group.
    std::array<wxString, kThemeKeywordGroupsCount> m_groups {}; ///< Per-group keyword text.
    std::array<CaseMode, kThemeKeywordGroupsCount> m_cases { CaseMode::None }; ///< Per-group case mode.
};

} // namespace fbide
