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
auto groupKey(std::size_t idx) -> std::string {
    return "group" + std::to_string(idx + 1);
}
} // namespace

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto groups = getContext().getConfigManager().keywords().at("groups");
    for (std::size_t idx = 0; idx < GROUP_COUNT; idx++) {
        m_groups.at(idx) = groups.get_or(groupKey(idx), "");
    }
}

void KeywordsPage::create() {
    // Group dropdown
    const wxArrayString options {
        tr("dialogs.settings.keywords.group1"),
        tr("dialogs.settings.keywords.group2"),
        tr("dialogs.settings.keywords.group3"),
        tr("dialogs.settings.keywords.group4")
    };
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

    // Persist to keywords toml via ConfigManager. Editors pick up changes
    // on UIManager::updateEditorSettigs() via Editor::applyTheme.
    auto& cfg = getContext().getConfigManager();
    auto groups = cfg.keywords()["groups"];
    for (std::size_t idx = 0; idx < GROUP_COUNT; idx++) {
        groups[groupKey(idx)] = m_groups[idx];
    }
    cfg.save(ConfigManager::Category::Keywords);
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
}
