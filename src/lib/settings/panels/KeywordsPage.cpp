//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "KeywordsPage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& keywords = getContext().getKeywords();
    for (int idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        m_groups[static_cast<size_t>(idx)] = keywords.getGroup(idx);
    }
}

void KeywordsPage::layout() {
    const auto& lang = getContext().getLang();

    // Dropdown
    m_groupChoice = make_unowned<wxChoice>(this, wxID_ANY);
    m_groupChoice->Append(lang[LangId::ThemeGroup1]);
    m_groupChoice->Append(lang[LangId::ThemeGroup2]);
    m_groupChoice->Append(lang[LangId::ThemeGroup3]);
    m_groupChoice->Append(lang[LangId::ThemeGroup4]);
    m_groupChoice->SetSelection(static_cast<int>(m_selectedGroup));
    getVBox()->Add(m_groupChoice.get(), 0, wxLEFT | wxTOP | wxRIGHT, 5);

    // Keyword text area
    m_textKeywords = make_unowned<wxTextCtrl>(
        this, wxID_ANY,
        m_groups[m_selectedGroup],
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_WORDWRAP
    );
    getVBox()->Add(m_textKeywords.get(), 1, wxEXPAND | wxALL, 5);
    m_groupChoice->Bind(wxEVT_CHOICE, &KeywordsPage::onGroupChanged, this);
}

void KeywordsPage::apply() {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    auto& keywords = getContext().getKeywords();
    for (int idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        keywords.setGroup(idx, m_groups[static_cast<size_t>(idx)]);
    }
    keywords.save();
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
}
