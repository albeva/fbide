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
#include "config/Keywords.hpp"
#include "config/Lang.hpp"
using namespace fbide;

namespace {
auto groupKey(std::size_t idx) -> std::string {
    return "group" + std::to_string(idx + 1);
}
} // namespace

KeywordsPage::KeywordsPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    // Load from keywords toml, fall back to runtime Keywords for unset groups
    const auto& keywordsToml = getContext().getConfigManager().getKeywords();
    const auto& runtime = getContext().getKeywords();
    const auto* groups = keywordsToml.is_table() && keywordsToml.contains("groups")
                       ? &keywordsToml.at("groups")
                       : nullptr;
    for (std::size_t idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        const auto key = groupKey(idx);
        if (groups != nullptr && groups->is_table() && groups->contains(key) && groups->at(key).is_string()) {
            m_groups.at(idx) = groups->at(key).as_string();
        } else {
            m_groups.at(idx) = runtime.getGroup(idx);
        }
    }
}

void KeywordsPage::create() {
    const auto& lang = getContext().getLang();

    // Group dropdown
    const wxArrayString options {
        lang[LangId::ThemeGroup1],
        lang[LangId::ThemeGroup2],
        lang[LangId::ThemeGroup3],
        lang[LangId::ThemeGroup4]
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

    // Persist to keywords toml via ConfigManager
    auto& cfg = getContext().getConfigManager();
    auto& keywordsToml = cfg.getKeywords();
    auto& groups = keywordsToml["groups"];
    for (std::size_t idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        groups[groupKey(idx)] = m_groups[idx].ToStdString();
    }
    cfg.save(ConfigManager::Category::Keywords);

    // Update runtime for syntax highlighting
    auto& keywords = getContext().getKeywords();
    for (std::size_t idx = 0; idx < Keywords::GROUP_COUNT; idx++) {
        keywords.setGroup(idx, m_groups[idx]);
    }
}

void KeywordsPage::onGroupChanged(const wxCommandEvent& event) {
    m_groups[m_selectedGroup] = m_textKeywords->GetValue();
    m_selectedGroup = static_cast<std::size_t>(event.GetSelection());
    m_textKeywords->SetValue(m_groups[m_selectedGroup]);
}
