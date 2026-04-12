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
    for (std::size_t idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        m_groups.at(idx) = keywords.getGroup(idx);
    }
}

void KeywordsPage::layout() {
    const auto& lang = getContext().getLang();

    // Group dropdown
    const std::vector options {
        lang[LangId::ThemeGroup1],
        lang[LangId::ThemeGroup2],
        lang[LangId::ThemeGroup3],
        lang[LangId::ThemeGroup4]
    };
    m_groupChoice = choice(options, { .flag = wxLEFT | wxTOP | wxRIGHT });
    m_groupChoice->SetSelection(static_cast<int>(m_selectedGroup));
    m_groupChoice->Bind(wxEVT_CHOICE, &KeywordsPage::onGroupChanged, this);

    // Keyword text area
    m_textKeywords = make_unowned<wxTextCtrl>(
        this, wxID_ANY,
        m_groups[m_selectedGroup],
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_WORDWRAP
    );
    getCurrentSizer()->Add(m_textKeywords, 1, wxEXPAND | wxALL, DEFAULT_PAD);
}

void KeywordsPage::apply() {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    auto& keywords = getContext().getKeywords();
    for (std::size_t idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        keywords.setGroup(idx, m_groups[static_cast<size_t>(idx)]);
    }
    keywords.save();
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
}
