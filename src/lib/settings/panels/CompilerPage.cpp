//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "CompilerPage.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "help/HelpManager.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

namespace {
/// Suffix applied to the active configuration's display name in the
/// list to make the current default obvious at a glance.
constexpr auto kActiveSuffix = " (active)";

/// Lift a `ResolvedCompilerConfig` field into the `wxString` form the
/// `InheritableField` widget expects. `path` is the only non-string
/// member; the others are templates we pass verbatim.
auto fieldValueOf(const ResolvedCompilerConfig& cfg, CompilerField field) -> wxString {
    switch (field) {
    case CompilerField::Path:
        return toWxString(cfg.path);
    case CompilerField::CompileCommand:
        return cfg.compileCommand;
    case CompilerField::RunCommand:
        return cfg.runCommand;
    case CompilerField::Terminal:
        return cfg.terminal;
    }
    return wxString {};
}

auto fieldKeyOf(const CompilerField field) -> wxString {
    switch (field) {
    case CompilerField::Path:
        return "path";
    case CompilerField::CompileCommand:
        return "compileCommand";
    case CompilerField::RunCommand:
        return "runCommand";
    case CompilerField::Terminal:
        return "terminal";
    }
    return wxString {};
}

/// Find the list-position of `slug` in `entries`, iterating rather
/// than indexing so the bounds check doesn't trip clang-tidy.
auto indexBySlug(const std::span<const ResolvedCompilerConfig> entries, const wxString& slug) -> int {
    int idx = 0;
    for (const auto& cfg : entries) {
        if (cfg.slug == slug) {
            return idx;
        }
        ++idx;
    }
    return wxNOT_FOUND;
}

/// Inverse: return the entry at `index`, or `nullptr` when out of range.
auto entryByIndex(const std::span<const ResolvedCompilerConfig> entries, const int index) -> const ResolvedCompilerConfig* {
    if (index < 0) {
        return nullptr;
    }
    int idx = 0;
    for (const auto& cfg : entries) {
        if (idx++ == index) {
            return &cfg;
        }
    }
    return nullptr;
}
} // namespace

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    m_helpFile = getContext().getConfigManager().config().get_or("paths.helpFile", "");
}

auto CompilerPage::catalog() const -> CompilerConfigCatalog& {
    return getContext().getCompilerManager().catalog();
}

void CompilerPage::create() {
    buildConfigurationsGroup();
    buildHelpGroup();
    refreshList();
    // Open with the active configuration selected — that's the entry
    // the user is most likely to want to inspect or tweak.
    m_selectedSlug = catalog().activeSlug();
    if (const auto idx = indexBySlug(catalog().all(), m_selectedSlug); idx != wxNOT_FOUND) {
        m_configList->SetSelection(idx);
    }
    loadSelectedConfig();
    SetSizerAndFit(currentSizer());
}

void CompilerPage::apply() {
    commitFieldOverrides();
    auto& cfg = getContext().getConfigManager().config();
    const wxString existingHelp = cfg.get_or("paths.helpFile", "");
    if (m_helpFile != existingHelp) {
        cfg["paths"]["helpFile"] = m_helpFile;
    }
    // resetFbcVersion in case the active config's path changed
    // (commitFieldOverrides will have written any path override above).
    getContext().getCompilerManager().resetFbcVersion();
}

void CompilerPage::focusCompilerPath() {
    if (m_pathField != nullptr) {
        m_pathField->SetFocus();
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CompilerPage::buildConfigurationsGroup() {
    hbox(tr("dialogs.settings.compiler.configurations"), { .proportion = 1, .margin = false }, [&] {
        buildLeftPane();
        buildRightPane();
    });
}

void CompilerPage::buildLeftPane() {
    vbox({ .margin = false }, [&] {
        m_configList = make_unowned<wxListBox>(
            currentParent(), wxID_ANY,
            wxDefaultPosition, wxDefaultSize,
            wxArrayString {}, wxLB_SINGLE
        );
        m_configList->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { onListSelected(); });
        add(m_configList, { .proportion = 1 });

        hbox({ .margin = false }, [&] {
            m_addButton = button(tr("dialogs.settings.compiler.add"));
            m_addButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onAddClicked(); });

            m_copyButton = button(tr("dialogs.settings.compiler.copy"));
            m_copyButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onCopyClicked(); });

            m_removeButton = button(tr("dialogs.settings.compiler.remove"));
            m_removeButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onRemoveClicked(); });
        });
    });
}

void CompilerPage::buildRightPane() {
    vbox({ .proportion = 2, .margin = false }, [&] {
        // Name + slug.
        m_nameLabel = text(tr("dialogs.settings.compiler.name"));
        m_nameField = textField();
        connect(m_nameLabel, m_nameField);
        m_nameField->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { onNameChanged(); });

        // Base dropdown.
        m_baseLabel = text(tr("dialogs.settings.compiler.base"));
        m_baseChoice = choice(wxArrayString {});
        connect(m_baseLabel, m_baseChoice);
        m_baseChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { onBaseChanged(); });

        // Active checkbox.
        m_activeCheckbox = checkBox(tr("dialogs.settings.compiler.activeForNewFiles"));
        m_activeCheckbox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { onActiveToggled(); });

        // Four inheritable fields. Each is its own panel — add() places
        // it into the surrounding vbox. The tooltip on each row's
        // inherit checkbox matches `ColorPicker`'s convention.
        const auto inheritTooltip = tr("dialogs.settings.compiler.inheritTooltip");
        auto buildField = [&](InheritableField::Kind kind, const wxString& labelKey) {
            auto field = make_unowned<InheritableField>(currentParent(), kind, tr(labelKey), inheritTooltip);
            field->create();
            add(field);
            return field;
        };
        m_pathField = buildField(InheritableField::Kind::Path, "dialogs.settings.compiler.path");
        m_compileField = buildField(InheritableField::Kind::Text, "dialogs.settings.compiler.compileCommand");
        m_runField = buildField(InheritableField::Kind::Text, "dialogs.settings.compiler.runCommand");
        m_terminalField = buildField(InheritableField::Kind::Text, "dialogs.settings.compiler.terminal");
    });
}

void CompilerPage::buildHelpGroup() {
    vbox(tr("dialogs.settings.compiler.help"), { .margin = false }, [&] {
        const auto lbl = text(tr("dialogs.settings.compiler.helpFile"));
        Unowned<wxTextCtrl> field;
        Unowned<wxButton> browse;
        hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
            field = textField(m_helpFile, { .proportion = 1 });
            connect(lbl, field);
            browse = button("...");
        });
        browse->Bind(wxEVT_BUTTON, [this, field](wxCommandEvent&) {
            wxFileDialog dlg(
                this, tr("dialogs.settings.compiler.selectHelp"), "", "",
                getContext().getConfigManager().filePattern("help"),
                wxFD_FILE_MUST_EXIST
            );
            if (dlg.ShowModal() == wxID_OK) {
                m_helpFile = getContext().getConfigManager().relative(dlg.GetPath());
#ifdef __WXMSW__
                HelpManager::verifyHelpFileAccessible(this, m_helpFile);
#endif
                field->SetValue(m_helpFile);
            }
        });
    });
}

// ---------------------------------------------------------------------------
// List + editor sync
// ---------------------------------------------------------------------------

void CompilerPage::refreshList() {
    m_configList->Clear();
    const auto activeSlug = catalog().activeSlug();
    for (const auto& cfg : catalog().all()) {
        wxString label = cfg.displayName;
        if (cfg.slug == activeSlug) {
            label += kActiveSuffix;
        }
        m_configList->Append(label);
    }
}

void CompilerPage::loadSelectedConfig() {
    const auto* cfg = catalog().find(m_selectedSlug);
    if (cfg == nullptr) {
        return;
    }
    const bool isCanonical = (cfg->slug == kCanonicalCompilerSlug);

    // Name + base are meaningless for canonical Default — hide them
    // entirely so the layout collapses. (Disabling alone leaves greyed
    // controls that look like a broken UI state.)
    m_nameLabel->Show(!isCanonical);
    m_nameField->Show(!isCanonical);
    if (!isCanonical) {
        m_nameField->ChangeValue(cfg->displayName);
    }

    m_baseLabel->Show(!isCanonical);
    m_baseChoice->Show(!isCanonical);
    if (!isCanonical) {
        m_baseChoice->Clear();
        m_baseChoiceSlugs.clear();
        for (const auto& candidate : catalog().validBasesFor(cfg->slug)) {
            const auto* candidateCfg = catalog().find(candidate);
            if (candidateCfg == nullptr) {
                continue;
            }
            m_baseChoice->Append(candidateCfg->displayName);
            m_baseChoiceSlugs.push_back(candidate);
        }
        const auto currentBase = getContext().getConfigManager().config().at("compiler").at(cfg->slug).get_or("base", wxString { kCanonicalCompilerSlug });
        const auto targetBase = currentBase.IsEmpty() ? wxString { kCanonicalCompilerSlug } : currentBase;
        if (const auto it = std::ranges::find(m_baseChoiceSlugs, targetBase);
            it != m_baseChoiceSlugs.end()) {
            m_baseChoice->SetSelection(static_cast<int>(std::distance(m_baseChoiceSlugs.begin(), it)));
        }
    }

    m_activeCheckbox->SetValue(cfg->slug == catalog().activeSlug());

    // Field state: an override is present when the raw `[compiler/<slug>]`
    // (or `[compiler]` for canonical) section carries the key. Resolved
    // values come from the catalog so they reflect the chain correctly.
    const auto& cfgSection = isCanonical
                               ? getContext().getConfigManager().config().at("compiler")
                               : getContext().getConfigManager().config().at("compiler").at(cfg->slug);
    auto loadField = [&](InheritableField* widget, CompilerField field) {
        const auto key = fieldKeyOf(field);
        const auto resolved = fieldValueOf(*cfg, field);
        const bool overridden = cfgSection.contains(key);
        widget->setResolvedValue(resolved);
        widget->setOverrideValue(overridden ? cfgSection.get_or(key, wxString {}) : resolved);
        // Canonical Default has no parent → no inherit checkbox. The
        // field becomes a plain editable input whose value is always
        // an explicit override.
        widget->setInheritCheckboxVisible(!isCanonical);
        if (!isCanonical) {
            widget->setInherited(!overridden);
        }
    };
    loadField(m_pathField.get(), CompilerField::Path);
    loadField(m_compileField.get(), CompilerField::CompileCommand);
    loadField(m_runField.get(), CompilerField::RunCommand);
    loadField(m_terminalField.get(), CompilerField::Terminal);

    m_removeButton->Enable(!isCanonical);
    m_copyButton->Enable(true);
    Layout();
}

void CompilerPage::commitFieldOverrides() {
    if (m_selectedSlug.IsEmpty() || catalog().find(m_selectedSlug) == nullptr) {
        return;
    }
    auto commit = [&](InheritableField* widget, CompilerField field) {
        const auto value = widget->isInherited()
                             ? std::optional<wxString> {}
                             : std::optional<wxString> { widget->overrideValue() };
        catalog().setOverride(m_selectedSlug, field, value);
    };
    commit(m_pathField.get(), CompilerField::Path);
    commit(m_compileField.get(), CompilerField::CompileCommand);
    commit(m_runField.get(), CompilerField::RunCommand);
    commit(m_terminalField.get(), CompilerField::Terminal);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void CompilerPage::onListSelected() {
    const auto sel = m_configList->GetSelection();
    if (sel == wxNOT_FOUND) {
        return;
    }
    commitFieldOverrides();
    const auto* selected = entryByIndex(catalog().all(), sel);
    if (selected == nullptr) {
        return;
    }
    m_selectedSlug = selected->slug;
    loadSelectedConfig();
}

void CompilerPage::onAddClicked() {
    commitFieldOverrides();

    // Required name — prompt, trim leading/trailing whitespace (including
    // newlines pasted in by accident), reject empty. wxString::Trim
    // honours wxIsspace which treats \n / \r / \t as whitespace.
    wxTextEntryDialog dlg(
        this,
        tr("dialogs.settings.compiler.namePrompt"),
        tr("dialogs.settings.compiler.addTitle")
    );
    while (dlg.ShowModal() == wxID_OK) {
        auto name = dlg.GetValue();
        name.Trim().Trim(false);
        if (name.IsEmpty()) {
            wxMessageBox(
                tr("dialogs.settings.compiler.nameRequired"),
                tr("dialogs.settings.compiler.addTitle"),
                wxICON_WARNING | wxOK,
                this
            );
            dlg.SetValue(wxEmptyString);
            continue;
        }
        const auto slug = catalog().createFromCanonical(name);
        refreshList();
        m_selectedSlug = slug;
        if (const auto idx = indexBySlug(catalog().all(), slug); idx != wxNOT_FOUND) {
            m_configList->SetSelection(idx);
        }
        loadSelectedConfig();
        m_nameField->SetFocus();
        m_nameField->SelectAll();
        getContext().getCompilerManager().refreshConfigurationCombo();
        return;
    }
}

void CompilerPage::onCopyClicked() {
    if (m_selectedSlug.IsEmpty()) {
        return;
    }
    commitFieldOverrides();
    const auto* source = catalog().find(m_selectedSlug);
    const auto sourceName = source != nullptr ? source->displayName : wxString {};
    const auto newName = sourceName + " " + tr("dialogs.settings.compiler.copySuffix");
    const auto slug = catalog().copy(m_selectedSlug, newName);
    refreshList();
    m_selectedSlug = slug;
    if (const auto idx = indexBySlug(catalog().all(), slug); idx != wxNOT_FOUND) {
        m_configList->SetSelection(idx);
    }
    loadSelectedConfig();
    getContext().getCompilerManager().refreshConfigurationCombo();
}

void CompilerPage::onRemoveClicked() {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    const auto confirm = wxMessageBox(
        tr("dialogs.settings.compiler.removeConfirm"),
        tr("dialogs.settings.compiler.removeTitle"),
        wxICON_QUESTION | wxYES_NO,
        this
    );
    if (confirm != wxYES) {
        return;
    }
    catalog().remove(m_selectedSlug);
    refreshList();
    m_selectedSlug = kCanonicalCompilerSlug;
    m_configList->SetSelection(0);
    loadSelectedConfig();
    getContext().getCompilerManager().refreshConfigurationCombo();
}

void CompilerPage::onNameChanged() {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    catalog().rename(m_selectedSlug, m_nameField->GetValue());
    // Just refresh the visible label without rebuilding the whole list
    // (rebuilding would steal the user's edit focus from the field).
    const auto sel = m_configList->GetSelection();
    if (sel == wxNOT_FOUND) {
        return;
    }
    wxString label = m_nameField->GetValue();
    if (m_selectedSlug == catalog().activeSlug()) {
        label += kActiveSuffix;
    }
    m_configList->SetString(static_cast<unsigned int>(sel), label);
    getContext().getCompilerManager().refreshConfigurationCombo();
}

void CompilerPage::onBaseChanged() {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    const auto sel = m_baseChoice->GetSelection();
    if (sel < 0 || static_cast<std::size_t>(sel) >= m_baseChoiceSlugs.size()) {
        return;
    }
    catalog().setBase(m_selectedSlug, m_baseChoiceSlugs.at(static_cast<std::size_t>(sel)));
    // Resolved values may have shifted (different ancestor); re-read the
    // four fields so inherited displays reflect the new chain.
    loadSelectedConfig();
}

void CompilerPage::onActiveToggled() {
    if (m_selectedSlug.IsEmpty()) {
        return;
    }
    catalog().setActiveSlug(m_activeCheckbox->GetValue() ? m_selectedSlug : wxString { kCanonicalCompilerSlug });
    refreshList();
    // Restore the selection — refreshList wiped it.
    if (const auto idx = indexBySlug(catalog().all(), m_selectedSlug); idx != wxNOT_FOUND) {
        m_configList->SetSelection(idx);
    }
    getContext().getCompilerManager().refreshConfigurationCombo();
}
