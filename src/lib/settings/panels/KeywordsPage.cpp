//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "KeywordsPage.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& keywords = getContext().getConfigManager().keywords();
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[idx]);
        m_groups[idx] = keywords.get_or(wxString(key), "");
    }
}

void KeywordsPage::create() {
    // Group dropdown
    wxArrayString options;
    options.reserve(kThemeKeywordGroupsCount);
    const auto& group = getContext().getConfigManager().locale().at("dialogs.settings.themes.categories");
    for (const auto& cat : kThemeKeywordCategories) {
        wxString key { getThemeCategoryName(cat) };
        key[0] = wxTolower(key[0]);
        options.Add(group.get_or(key, key));
    }

    m_groupChoice = choice(options, { .expand = false });
    m_groupChoice->SetSelection(static_cast<int>(m_selectedGroup));
    m_groupChoice->Bind(wxEVT_CHOICE, &KeywordsPage::onGroupChanged, this);

    // Keyword text area
    m_textKeywords = make_unowned<wxTextCtrl>(
        currentParent(), wxID_ANY,
        m_groups[m_selectedGroup],
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_WORDWRAP
    );

    wxFont font = m_textKeywords->GetFont();
    font.SetFamily(wxFONTFAMILY_TELETYPE);
    m_textKeywords->SetFont(font);

    add(m_textKeywords, { .proportion = 1 });
    spacer();
}

void KeywordsPage::apply() {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();

    auto& cfg = getContext().getConfigManager();
    auto& keywords = cfg.keywords();
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[idx]);
        keywords[wxString(key)] = m_groups[idx];
    }
    cfg.save(ConfigManager::Category::Keywords);
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
}
