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

namespace {

auto caseModeChoices() -> wxArrayString {
    wxArrayString names;
    for (const auto value : CaseMode::all) {
        names.Add(wxString::FromUTF8(CaseMode { value }.toString()));
    }
    return names;
}

} // namespace

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& keywords = getContext().getConfigManager().keywords();
    const auto& groups = keywords.at("groups");
    const auto& cases = keywords.at("cases");
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
        m_groups[idx] = groups.get_or(key, "");
        m_cases[idx] = CaseMode::parse(cases.get_or(key, "None").ToStdString())
                           .value_or(CaseMode::None);
    }
}

void KeywordsPage::create() {
    // Group dropdown + case dropdown row
    hbox({ .center = true, .border = 0 }, [&] {
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

        m_caseChoice = choice(caseModeChoices(), { .expand = false });
        m_caseChoice->SetSelection(static_cast<int>(m_cases[m_selectedGroup].value()));
        m_caseChoice->SetMinSize(wxSize(120, -1));
        m_caseChoice->Bind(wxEVT_CHOICE, &KeywordsPage::onCaseChanged, this);
    });

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
    SetSizerAndFit(currentSizer());
}

void KeywordsPage::stashCurrent() {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_cases[m_selectedGroup] = static_cast<CaseMode::Value>(m_caseChoice->GetSelection());
}

void KeywordsPage::apply() {
    stashCurrent();

    auto& cfg = getContext().getConfigManager();
    auto& keywords = cfg.keywords();
    auto& groups = keywords["groups"];
    auto& cases = keywords["cases"];
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
        groups[key] = m_groups[idx];
        cases[key] = wxString::FromUTF8(m_cases[idx].toString());
    }
    cfg.save(ConfigManager::Category::Keywords);
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    stashCurrent();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
    m_caseChoice->SetSelection(static_cast<int>(m_cases[m_selectedGroup].value()));
}

void KeywordsPage::onCaseChanged(const wxCommandEvent& event) {
    m_cases[m_selectedGroup] = static_cast<CaseMode::Value>(event.GetSelection());
}
