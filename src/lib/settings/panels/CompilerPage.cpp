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
#include "compiler/FbcAutoDetect.hpp"
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
constexpr int ID_ACTIVE = wxID_HIGHEST + 205;
constexpr int ID_SHOW_IN_MENU = wxID_HIGHEST + 206;
constexpr int ID_MOVE_UP = wxID_HIGHEST + 207;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 208;

/// Width of the left-hand configuration list in device-independent
/// pixels. Sized to comfortably show "FBC 32bit GUI (active)" without
/// horizontal scroll while leaving room for the field editors.
constexpr int kListWidth = 150;

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

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(CompilerPage, Panel)
    EVT_LISTBOX (ID_LIST,   CompilerPage::onListSelChanged)
    EVT_BUTTON  (ID_ADD,    CompilerPage::onAddClicked)
    EVT_BUTTON  (ID_COPY,   CompilerPage::onCopyClicked)
    EVT_BUTTON  (ID_REMOVE, CompilerPage::onRemoveClicked)
    EVT_BUTTON  (ID_MOVE_UP,   CompilerPage::onMoveUpClicked)
    EVT_BUTTON  (ID_MOVE_DOWN, CompilerPage::onMoveDownClicked)
    EVT_TEXT    (ID_NAME,   CompilerPage::onNameChanged)
    EVT_CHECKBOX(ID_ACTIVE, CompilerPage::onActiveToggled)
    EVT_CHECKBOX(ID_SHOW_IN_MENU, CompilerPage::onShowInMenuToggled)
    EVT_COMMAND (wxID_ANY,  EVT_INHERIT_TOGGLED, CompilerPage::onInheritToggled)
wxEND_EVENT_TABLE()
// clang-format on

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_locale(ctx.getConfigManager().locale().at("dialogs.settings.compiler")) {}

auto CompilerPage::catalog() const -> CompilerConfigCatalog& {
    return getContext().getCompilerManager().catalog();
}

auto CompilerPage::fieldEntries() const
    -> std::array<std::pair<InheritableField*, CompilerField>, kAllCompilerFields.size()> {
    return { {
        { m_pathField, CompilerField::Path },
        { m_compileField, CompilerField::CompileCommand },
        { m_runField, CompilerField::RunCommand },
        { m_terminalField, CompilerField::Terminal },
    } };
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

auto CompilerPage::validate() -> bool {
    // Commit the editing fields into the catalog before checking: if
    // the user edited a field on the active row and left the name
    // empty, we still want their edit preserved when we focus the bad
    // row below. The catalog is staged state (restored on `cancel()`),
    // so committing here has no externally visible effect.
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
    return true;
}

void CompilerPage::apply() {
    // `validate()` already committed the field overrides; commit again
    // so `apply()` stands on its own. `commitFieldOverrides()` is
    // idempotent, so the repeat is harmless.
    commitFieldOverrides();
    getContext().getCompilerManager().refreshConfigurationCombo();
}

void CompilerPage::cancel() {
    // Restore the original `[compiler]` tree and have everything that
    // reads it refresh — catalog cache, toolbar combobox.
    auto& compiler = getContext().getConfigManager().config()["compiler"];
    compiler = m_compilerSnapshot.clone();
    getContext().getCompilerManager().catalog().reload();
    getContext().getCompilerManager().refreshConfigurationCombo();
}

void CompilerPage::focusPath(const wxString& path) {
    // path = "<config-slug>/<field>"; both segments optional.
    const wxString slug = path.BeforeFirst('/');
    const wxString fieldKey = path.AfterFirst('/');

    // Select the requested configuration when given and known —
    // otherwise leave the active selection create() set up.
    if (!slug.IsEmpty() && catalog().find(slug) != nullptr && slug != m_selectedSlug) {
        commitFieldOverrides();
        m_selectedSlug = slug;
        selectSlug(slug);
        loadSelectedConfig();
    }

    // Focus the requested field, defaulting to the compiler path.
    const auto target = compilerFieldFromKey(fieldKey).value_or(CompilerField::Path);
    for (const auto& [widget, field] : fieldEntries()) {
        if (field == target && widget != nullptr) {
            widget->focusField();
            return;
        }
    }
}

void CompilerPage::autoDetect() {
#ifdef __WXMSW__
    auto generated = FbcAutoDetect(getContext()).run(this);
    if (!generated.has_value()) {
        return; // Cancelled, or nothing valid found (error already shown).
    }
    // Install the detected `[compiler]` subtree wholesale. The snapshot
    // create() took still covers rollback if the user later hits Cancel.
    getContext().getConfigManager().config()["compiler"] = std::move(*generated);
    catalog().reload();
    refreshList();
    m_selectedSlug = catalog().activeSlug();
    selectSlug(m_selectedSlug);
    loadSelectedConfig();
    getContext().getCompilerManager().refreshConfigurationCombo();
#endif
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
            wxDefaultPosition, wxSize(kListWidth, -1),
            wxArrayString {}, wxLB_SINGLE
        );
        add(m_configList, { .proportion = 1 });

        hbox({ .margin = false }, [&] {
            auto makeIconButton = [this](int controlId, const wxArtID& artId, const wxString& tooltipKey) {
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
            m_moveUpButton = makeIconButton(ID_MOVE_UP, wxART_GO_UP, "moveUp");
            m_moveDownButton = makeIconButton(ID_MOVE_DOWN, wxART_GO_DOWN, "moveDown");
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

        // Active checkbox.
        m_activeCheckbox = checkBox(tr("activeForNewFiles"), {}, ID_ACTIVE);

        // Show-in-menu checkbox — controls whether this configuration
        // appears in the toolbar combobox / status-bar selector.
        m_showInMenuCheckbox = checkBox(tr("showInMenu"), {}, ID_SHOW_IN_MENU);

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
    if (const auto index = catalog().indexOf(slug); index >= 0) {
        m_configList->SetSelection(index);
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
    // The per-field "last override" memory is per-configuration —
    // switching configs starts fresh.
    m_lastOverrideValues.clear();
    const bool isCanonical = (cfg->slug == kCanonicalCompilerSlug);

    // Name is meaningless for canonical Default — hide it entirely so
    // the layout collapses. (Disabling alone leaves greyed controls
    // that look like a broken UI state.)
    m_nameLabel->Show(!isCanonical);
    m_nameField->Show(!isCanonical);
    if (!isCanonical) {
        m_nameField->ChangeValue(cfg->displayName);
    }

    // Active is a "promote this row" tick — the only way to clear an
    // active row is to promote a different one. Disabling on the
    // already-active row removes the footgun of unticking-by-accident
    // (which would otherwise demote the row back to canonical).
    const bool isActiveRow = (cfg->slug == catalog().activeSlug());
    m_activeCheckbox->SetValue(isActiveRow);
    m_activeCheckbox->Enable(!isActiveRow);

    // Show-in-menu reflects the per-config visibility flag — default true.
    m_showInMenuCheckbox->SetValue(cfg->showInMenu);

    // Field state: an override is present when the raw `[compiler/<slug>]`
    // (or `[compiler]` for canonical) section carries the key. Resolved
    // values come from the catalog so they reflect what canonical would
    // supply if the field were left to inherit.
    const auto& cfgSection = isCanonical
                               ? getContext().getConfigManager().config().at("compiler")
                               : getContext().getConfigManager().config().at("compiler").at(cfg->slug);

    // What this config would inherit if every field were unset — the
    // resolved fields of canonical Default. Shown in the disabled
    // input box while the inherit checkbox is ticked so the user can
    // see exactly what they're falling back to.
    const ResolvedCompilerConfig* canonicalCfg = isCanonical ? nullptr : &catalog().canonical();

    auto loadField = [&](InheritableField* widget, const CompilerField field) {
        const auto key = compilerFieldKey(field);
        const bool overridden = cfgSection.contains(key);
        const auto inheritedValue = canonicalCfg != nullptr
                                      ? fieldValueOf(*canonicalCfg, field)
                                      : fieldValueOf(*cfg, field);
        widget->setResolvedValue(inheritedValue);
        widget->setOverrideValue(overridden ? cfgSection.get_or(key, wxString {}) : inheritedValue);
        // Canonical Default has nothing to inherit from → no inherit
        // checkbox. The field becomes a plain editable input whose
        // value is always an explicit override.
        widget->setInheritCheckboxVisible(!isCanonical);
        if (!isCanonical) {
            widget->setInherited(!overridden);
        }
    };
    for (const auto& [widget, field] : fieldEntries()) {
        loadField(widget, field);
    }

    m_removeButton->Enable(!isCanonical);
    m_copyButton->Enable(true);

    // Up / Down are disabled for canonical Default (fixed at index 0)
    // and at the user-list boundaries.
    const auto selectionIndex = catalog().indexOf(cfg->slug);
    const auto lastIndex = static_cast<int>(catalog().all().size()) - 1;
    m_moveUpButton->Enable(!isCanonical && selectionIndex > 1);
    m_moveDownButton->Enable(!isCanonical && selectionIndex >= 0 && selectionIndex < lastIndex);

    GetSizer()->Layout();
}

void CompilerPage::commitFieldOverrides() {
    if (m_selectedSlug.IsEmpty() || catalog().find(m_selectedSlug) == nullptr) {
        return;
    }
    for (const auto& [widget, field] : fieldEntries()) {
        const auto value = widget->isInherited()
                             ? std::optional<wxString> {}
                             : std::optional<wxString> { widget->overrideValue() };
        catalog().setOverride(m_selectedSlug, field, value);
    }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void CompilerPage::onListSelChanged(wxCommandEvent& event) {
    event.Skip();
    const auto* cfg = catalog().at(m_configList->GetSelection());
    if (cfg == nullptr) {
        return;
    }
    // Programmatic SetSelection after Add / Copy / refresh fires this
    // handler too; if the slug already matches we leave the right-pane
    // alone (commitFieldOverrides would write the old widget state into
    // the freshly-created config otherwise).
    if (cfg->slug == m_selectedSlug) {
        return;
    }
    commitFieldOverrides();
    m_selectedSlug = cfg->slug;
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
    m_selectedSlug = kCanonicalCompilerSlug;
    refreshList();
    selectSlug(m_selectedSlug);
    loadSelectedConfig();
}

void CompilerPage::onNameChanged(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    catalog().rename(m_selectedSlug, m_nameField->GetValue());
    // Just refresh the visible label without rebuilding the whole list
    // (rebuilding would steal the user's edit focus from the field).
    if (const auto index = catalog().indexOf(m_selectedSlug); index >= 0) {
        m_configList->SetString(static_cast<unsigned>(index), formatListLabel(m_selectedSlug, m_nameField->GetValue()));
    }
}

void CompilerPage::onInheritToggled(wxCommandEvent& event) {
    auto* widget = wxDynamicCast(event.GetEventObject(), InheritableField);
    if (widget == nullptr) {
        return;
    }
    // Match the firing widget to its CompilerField via the shared
    // entries table — keeps this handler in sync with the load/commit
    // loops above without re-listing the four fields here.
    const auto entries = fieldEntries();
    const auto it = std::ranges::find(entries, widget, &std::pair<InheritableField*, CompilerField>::first);
    if (it == entries.end()) {
        return;
    }
    const auto field = it->second;

    const bool nowInheriting = event.GetInt() != 0;
    if (nowInheriting) {
        // User just ticked inherit — remember what they had so an
        // accidental tick can be undone on the next untick. The
        // InheritableField doesn't reset its m_overrideValue on the
        // tick path, so `overrideValue()` still returns their prior
        // edit at this point.
        m_lastOverrideValues[field] = widget->overrideValue();
    } else if (const auto remembered = m_lastOverrideValues.find(field);
        remembered != m_lastOverrideValues.end()) {
        // User just unticked — restore the prior value over the top
        // of the InheritableField's default "seed from resolved"
        // behaviour.
        widget->setOverrideValue(remembered->second);
    }
}

void CompilerPage::onMoveUpClicked(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    commitFieldOverrides();
    if (catalog().moveUp(m_selectedSlug)) {
        refreshList();
        selectSlug(m_selectedSlug);
        loadSelectedConfig();
    }
}

void CompilerPage::onMoveDownClicked(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty() || m_selectedSlug == kCanonicalCompilerSlug) {
        return;
    }
    commitFieldOverrides();
    if (catalog().moveDown(m_selectedSlug)) {
        refreshList();
        selectSlug(m_selectedSlug);
        loadSelectedConfig();
    }
}

void CompilerPage::onShowInMenuToggled(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty()) {
        return;
    }
    catalog().setShowInMenu(m_selectedSlug, m_showInMenuCheckbox->GetValue());
}

void CompilerPage::onActiveToggled(wxCommandEvent& /*event*/) {
    if (m_selectedSlug.IsEmpty() || !m_activeCheckbox->GetValue()) {
        return;
    }
    // Promotion only: the checkbox is disabled while it's already
    // checked (see loadSelectedConfig), so this handler only fires on
    // tick-on. The previous active row is implicitly demoted by
    // setActiveSlug.
    catalog().setActiveSlug(m_selectedSlug);
    refreshList();
    selectSlug(m_selectedSlug);
    // Re-disable the checkbox now that this row owns "active" — the
    // programmatic re-selection above no-ops the listbox handler.
    loadSelectedConfig();
}
