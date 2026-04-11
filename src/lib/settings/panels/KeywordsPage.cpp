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

namespace {
constexpr int border = 5;
}

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {}

void KeywordsPage::layout() {
    const auto& lang = getContext().getLang();
    const auto& keywords = getContext().getKeywords();

    for (int idx = 0; idx < 4; idx++) {
        m_groups[static_cast<size_t>(idx)] = keywords.getGroup(idx);
    }

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    const auto groupSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    groupSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeSelectGroup]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
    m_chKeywordGroup = make_unowned<wxChoice>(this, wxID_ANY);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup1]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup2]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup3]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup4]);
    m_chKeywordGroup->SetSelection(0);
    groupSizer->Add(m_chKeywordGroup.get(), 1);
    sizer->Add(groupSizer, 0, wxEXPAND | wxALL, border);

    m_textKeywords = make_unowned<wxTextCtrl>(this, wxID_ANY, m_groups[0],
        wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_WORDWRAP);
    sizer->Add(m_textKeywords.get(), 1, wxEXPAND | wxALL, border);

    SetSizer(sizer);
    m_chKeywordGroup->Bind(wxEVT_CHOICE, &KeywordsPage::onGroupChanged, this);
}

void KeywordsPage::apply() {
    m_groups[static_cast<size_t>(m_groupOld)] = m_textKeywords->GetValue();
    auto& keywords = getContext().getKeywords();
    for (int idx = 0; idx < 4; idx++) {
        keywords.setGroup(idx, m_groups[static_cast<size_t>(idx)]);
    }
    keywords.save();
}

void KeywordsPage::onGroupChanged(wxCommandEvent& event) {
    m_groups[static_cast<size_t>(m_groupOld)] = m_textKeywords->GetValue();
    m_groupOld = event.GetSelection();
    m_textKeywords->SetValue(m_groups[static_cast<size_t>(m_groupOld)]);
}
