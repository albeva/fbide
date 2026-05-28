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
#include "utils/PathConversions.hpp"
using namespace fbide;

namespace {
/// Stable IDs for this panel's interactive controls. Offset clear of
/// the InheritableField / ColorPicker ranges so a child control's
/// event can't accidentally match one of ours via parent-chain
/// propagation.
constexpr int ID_LIST = wxID_HIGHEST + 200;
constexpr int ID_ADD = wxID_HIGHEST + 201;
constexpr int ID_COPY = wxID_HIGHEST + 202;
constexpr int ID_REMOVE = wxID_HIGHEST + 203;
constexpr int ID_NAME = wxID_HIGHEST + 204;
constexpr int ID_BASE = wxID_HIGHEST + 205;
constexpr int ID_ACTIVE = wxID_HIGHEST + 206;

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

// clang-format off
wxBEGIN_EVENT_TABLE(CompilerPage, Panel)
    EVT_LISTBOX (ID_LIST,   CompilerPage::onListSelected)
    EVT_BUTTON  (ID_ADD,    CompilerPage::onAddClicked)
    EVT_BUTTON  (ID_COPY,   CompilerPage::onCopyClicked)
    EVT_BUTTON  (ID_REMOVE, CompilerPage::onRemoveClicked)
    EVT_TEXT    (ID_NAME,   CompilerPage::onNameChanged)
    EVT_CHOICE  (ID_BASE,   CompilerPage::onBaseChanged)
    EVT_CHECKBOX(ID_ACTIVE, CompilerPage::onActiveToggled)
wxEND_EVENT_TABLE()
// clang-format on

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_locale(ctx.getConfigManager().locale().at("dialogs.settings.compiler")) {}

auto CompilerPage::catalog() const -> CompilerConfigCatalog& {
    return getContext().getCompilerManager().catalog();
}

void CompilerPage::create() {
    // Snapshot the catalog state up-front. Every edit during the dialog
    // session mutates the live catalog (so the rest of the UI stays
    // consistent), and `cancel()` swaps the snapshot back in if the
    // user bails.
    m_compilerSnapshot = getContext().getConfigManager().config().at("compiler").clone();

    buildConfigurationsGroup();
    SetSizerAndFit(currentSizer());

    refreshList();
    // Open with the active configuration selected — that's the entry
    // the user is most likely to want to inspect or tweak.
    m_selectedSlug = catalog().activeSlug();
    selectSlug(m_selectedSlug);
    loadSelectedConfig();
}

auto CompilerPage::apply() -> bool {
    commitFieldOverrides();

    // Required name on every user configuration. Trimmed so a
    // whitespace-only name still counts as empty.
    for (const auto& cfg : catalog().all()) {
        if (cfg.slug == kCanonicalCompilerSlug) {
            continue;
        }
        auto trimmed = cfg.displayName;
        trimmed.Trim().Trim(false);
        if (trimmed.IsEmpty()) {
            m_selectedSlug = cfg.slug;
            selectSlug(cfg.slug);
            loadSelectedConfig();
            m_nameField->SetFocus();
            wxMessageBox(
                tr("nameRequired"),
                tr("addTitle"),
                wxICON_WARNING | wxOK,
                this
            );
            return false;
        }
    }

    getContext().getCompilerManager().refreshConfigurationCombo();
    return true;
}

void CompilerPage::cancel() {
    // Restore the original `[compiler]` tree and have everything that
    // reads it refresh — catalog cache, toolbar combobox.
    auto& compiler = getContext().getConfigManager().config()["compiler"];
    compiler = m_compilerSnapshot.clone();
    getContext().getCompilerManager().catalog().reload();
    getContext().getCompilerManager().refreshConfigurationCombo();
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
    hbox(tr("configurations"), { .proportion = 1, .margin = false }, [&] {
        buildLeftPane();
        buildRightPane();
    });
}

void CompilerPage::buildLeftPane() {
    vbox({ .margin = false }, [&] {
        m_configList = make_unowned<wxListBox>(
            currentParent(), ID_LIST,
            wxDefaultPosition, wxDefaultSize,
            wxArrayString {}, wxLB_SINGLE
        );
        add(m_configList, { .proportion = 1 });

        hbox({ .margin = false }, [&] {
            auto makeIconButton = [this](int controlId, wxArtID artId, const wxString& tooltipKey) {
                auto btn = make_unowned<wxBitmapButton>(
                    currentParent(), controlId,
                    wxArtProvider::GetBitmap(artId, wxART_BUTTON)
                );
                btn->SetToolTip(tr(tooltipKey));
                add(btn);
                return btn;
            };
            m_addButton = makeIconButton(ID_ADD, wxART_NEW, "add");
            m_copyButton = makeIconButton(ID_COPY, wxART_COPY, "copy");
            m_removeButton = makeIconButton(ID_REMOVE, wxART_DELETE, "remove");
        });
    });
}

void CompilerPage::buildRightPane() {
    vbox({ .proportion = 1, .margin = false }, [&] {
        // Name.
        vbox({ .margin = false }, [&] {
            m_nameLabel = text(tr("name"));
            m_nameField = textField({}, ID_NAME);
            connect(m_nameLabel, m_nameField);
        });

        // Base dropdown.
        hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
            m_baseLabel = text(tr("base"), { .expand = false });
            m_baseChoice = choice(wxArrayString {}, { .expand = false }, ID_BASE);
            connect(m_baseLabel, m_baseChoice);
        });

        // Active checkbox.
        m_activeCheckbox = checkBox(tr("activeForNewFiles"), {}, ID_ACTIVE);

        // Four inheritable fields. Each is its own panel — add() places
        // it into the surrounding vbox. The tooltip on each row's
        // inherit checkbox matches `ColorPicker`'s convention.
        const auto inheritTooltip = tr("inheritTooltip");
        auto buildField = [&](InheritableField::Kind kind, const wxString& labelKey) {
            auto field = make_unowned<InheritableField>(currentParent(), kind, tr(labelKey), inheritTooltip);
            field->create();
            add(field);
            return field;
        };
        m_pathField = buildField(InheritableField::Kind::Path, "path");
        m_compileField = buildField(InheritableField::Kind::Text, "compileCommand");
        m_runField = buildField(InheritableField::Kind::Text, "runCommand");
        m_terminalField = buildField(InheritableField::Kind::Text, "terminal");
    });
}

// ---------------------------------------------------------------------------
// List + editor sync
// ---------------------------------------------------------------------------

auto CompilerPage::formatListLabel(const wxString& slug, const wxString& name) const -> wxString {
    if (slug == catalog().activeSlug()) {
        return wxString::Format("%s (%s)", name, tr("active"));
    }
    return name;
}

void CompilerPage::selectSlug(const wxString& slug) {
    if (const auto idx = indexBySlug(catalog().all(), slug); idx != wxNOT_FOUND) {
        m_configList->SetSelection(idx);
    }
}

void CompilerPage::refreshList() {
    m_configList->Clear();
    for (const auto& cfg : catalog().all()) {
        m_configList->Append(formatListLabel(cfg.slug, cfg.displayName));
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

    auto loadField = [&](InheritableField* widget, const CompilerField field) {
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
    loadField(m_pathField, CompilerField::Path);
    loadField(m_compileField, CompilerField::CompileCommand);
    loadField(m_runField, CompilerField::RunCommand);
    loadField(m_terminalField, CompilerField::Terminal);

    m_removeButton->Enable(!isCanonical);
    m_copyButton->Enable(true);

    GetSizer()->Layout();
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
    commit(m_pathField, CompilerField::Path);
    commit(m_compileField, CompilerField::CompileCommand);
    commit(m_runField, CompilerField::RunCommand);
    commit(m_terminalField, CompilerField::Terminal);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void CompilerPage::onListSelected(wxCommandEvent& /*event*/) {
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

void CompilerPage::onAddClicked(wxCommandEvent& /*event*/) {
    commitFieldOverrides();

    // Required name — prompt, trim leading/trailing whitespace (including
    // newlines pasted in by accident), reject empty. wxString::Trim
    // honours wxIsspace which treats \n / \r / \t as whitespace.
    wxTextEntryDialog dlg(this, tr("namePrompt"), tr("addTitle"));
    while (dlg.ShowModal() == wxID_OK) {
        auto name = dlg.GetValue();
        name.Trim().Trim(false);
        if (name.IsEmpty()) {
            wxMessageBox(tr("nameRequired"), tr("addTitle"), wxICON_WARNING | wxOK, this);
            dlg.SetValue(wxEmptyString);
            continue;
        }
        m_selectedSlug = catalog().createFromCanonical(name);
        refreshList();
        selectSlug(m_selectedSlug);
        loadSelectedConfig();
        return;
    }
}

void CompilerPage::onCopyClicked(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty()) {
        return;
    }
    commitFieldOverrides();
    const auto* source = catalog().find(m_selectedSlug);
    const auto sourceName = source != nullptr ? source->displayName : wxString {};
    const auto newName = sourceName + " " + tr("copySuffix");
    m_selectedSlug = catalog().copy(m_selectedSlug, newName);
    refreshList();
    selectSlug(m_selectedSlug);
    loadSelectedConfig();
}

void CompilerPage::onRemoveClicked(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    const auto confirm = wxMessageBox(
        tr("removeConfirm"),
        tr("removeTitle"),
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
}

void CompilerPage::onNameChanged(wxCommandEvent& /*event*/) {
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
    m_configList->SetString(
        static_cast<unsigned int>(sel),
        formatListLabel(m_selectedSlug, m_nameField->GetValue())
    );
}

void CompilerPage::onBaseChanged(wxCommandEvent& /*event*/) {
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

void CompilerPage::onActiveToggled(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty()) {
        return;
    }
    catalog().setActiveSlug(m_activeCheckbox->GetValue() ? m_selectedSlug : wxString { kCanonicalCompilerSlug });
    refreshList();
    selectSlug(m_selectedSlug);
}
