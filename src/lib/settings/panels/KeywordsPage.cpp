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

/// Build the localized labels for the case-transform dropdown.
/// Falls back to the stable INI key when the locale has no entry,
/// so a missing translation is still readable.
auto caseModeChoices(Context& ctx) -> wxArrayString {
    const auto& tr = ctx.getConfigManager().locale().at("dialogs.settings.keywords.case");
    wxArrayString names;
    for (const auto value : CaseMode::all) {
        wxString key = wxString::FromUTF8(CaseMode { value }.toString());
        key[0] = wxTolower(key[0]);
        names.Add(tr.get_or(key, key));
    }
    return names;
}

/// Labels in the keyword-group dropdown mirror the Themes-page tree:
/// keyword categories that live under the implicit Default top-level
/// drop the prefix; categories under Asm or Preprocessor combine the
/// top-level label with their leaf label, e.g. `Asm Instructions`,
/// `Preprocessor Directives`. Returned keys index `categories.*` in
/// the locale; an empty `topKey` means "no prefix".
struct GroupLabel {
    wxString topKey;
    wxString leafKey;
};

auto groupLabelKeys(const ThemeCategory cat) -> GroupLabel {
    using TC = ThemeCategory;
    switch (cat) {
    case TC::Keywords:
        return { "", "core" };
    case TC::KeywordTypes:
        return { "", "types" };
    case TC::KeywordOperators:
        return { "", "operators" };
    case TC::KeywordConstants:
        return { "", "defines" };
    case TC::KeywordLibrary:
        return { "", "library" };
    case TC::KeywordCustom:
        return { "", "custom" };
    case TC::KeywordAsm1:
        return { "asm", "instructions" };
    case TC::KeywordAsm2:
        return { "asm", "registers" };
    case TC::KeywordPP:
        return { "preprocessor", "directives" };
    default:
        return { "", "" };
    }
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
    hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
        wxArrayString options;
        options.reserve(kThemeKeywordGroupsCount);
        const auto& group = getContext().getConfigManager().locale().at("dialogs.settings.themes.categories");
        for (const auto& cat : kThemeKeywordCategories) {
            const auto labels = groupLabelKeys(cat);
            const wxString leaf = group.get_or(labels.leafKey, labels.leafKey);
            if (labels.topKey.IsEmpty()) {
                options.Add(leaf);
            } else {
                const wxString top = group.get_or(labels.topKey, labels.topKey);
                options.Add(top + " " + leaf);
            }
        }

        m_groupChoice = choice(options, { .expand = false });
        m_groupChoice->SetSelection(static_cast<int>(m_selectedGroup));
        m_groupChoice->Bind(wxEVT_CHOICE, &KeywordsPage::onGroupChanged, this);

        m_caseChoice = choice(caseModeChoices(getContext()), { .expand = false });
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
