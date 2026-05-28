# Multiple Compiler Configurations — Design

Status: spec, not yet implemented.
Branch: `compiler-configurations`.

## Goal

Let users define multiple FBC compiler configurations (32/64-bit, GUI/console,
custom flags, alternate compiler binaries) and select one per open document.
Backward compatible with existing single-`[compiler]` installs and existing
session files.

## Concepts

- **Canonical Default** — the long-standing `[compiler]` section. Cannot be
  removed. Reserved slug `default`. Backward compatible with existing
  `config_{platform}.ini` files.
- **User-defined configuration** — a named `[compiler/<slug>]` section that
  inherits from a base configuration (canonical or another user config) and
  may override any field.
- **Active configuration** — the slug stored in `compiler.active`. This is the
  configuration used when a document has no explicit pinned configuration.
  Defaults to canonical Default. Persisted globally, not per-document.
- **Document configuration** — `std::optional<wxString>` stored on each
  `Document`. Empty means "follow whichever configuration is active". A slug
  means "pin to this configuration regardless of active".

## Resolution rule

For any compile/run/quickRun on a document `D`:

1. If `D.configuration.has_value()` and the slug exists in the catalog → use it.
2. If `D.configuration.has_value()` but the slug is missing → fall back to
   the active configuration. Emit `wxLogWarning` once. Do **not** rewrite
   `D.configuration` automatically — let the session save lifecycle handle it
   (next save writes the absent key, or the user re-picks).
3. If `D.configuration` is empty → use the active configuration
   (`catalog.activeSlug()`).
4. If the active slug itself is missing/invalid → use canonical Default.

The "matches active → empty" normalization is enforced by `CompilerManager`
when the user picks a value in the toolbar combobox:

```cpp
const auto active = catalog.activeSlug();
doc.setConfiguration(pickedSlug == active
                       ? std::nullopt
                       : std::optional{wxString{pickedSlug}});
```

This means picking "Default" when active is `cfg-1` stores `"default"`
(pinned). Picking "Default" when active is also `default` (or unset) stores
empty. Same UI action, different state — by design, so that "follow active"
is opt-in.

## Slug scheme

- Canonical reserves `default`.
- User-created configs receive sequential opaque slugs: `cfg-1`, `cfg-2`, ...
- Counter persisted as `compiler.nextSlugIndex` in `[compiler]`. Monotonic;
  never reused. Default value `1`.
- Slugs are **immutable** after creation. `name` is freely editable; slug is
  shown read-only beneath the name field.
- INI-safe by construction; no normalization of user-entered names needed.

## Inheritance

- **Unlimited depth.** Example chain: Default → fbc-32 → fbc-32-gui.
- **Cycle prevention on write** (`setBase`): rejects if `newBase == slug` or
  if `newBase` is a transitive descendant of `slug`.
- **Cycle / orphan tolerance on read** (`reload`): a config whose `base` is
  unknown, or whose chain loops back on itself, resolves field-by-field
  against canonical Default. The config remains in the catalog (so the UI
  can show it and let the user fix it). `wxLogWarning` names the slug and
  reason.
- **Settings dropdown**: populated by `catalog.validBasesFor(slug)` — excludes
  self and all transitive descendants. Users can never pick a bad base via UI.

## Override semantics (per field)

Per-field override mirrors the theme system:

- Key **absent** under `[compiler/<slug>]` → inherit from base.
- Key **present** (even with empty value) → override, used as-is.
- Settings dialog toggles the key's presence via a checkbox per field.
  Unticking removes the key from the `Value` tree; ConfigManager's overlay
  diff then doesn't persist it.

This way the user can deliberately override a non-empty inherited value with
an explicit empty string.

## Config schema

```ini
[compiler]                            ; canonical, slug = "default"
path=/opt/fbc/bin/fbc
runCommand=<$file>
compileCommand="<$fbc>" "<$file>"
terminal=
active=cfg-2                          ; new — slug of active config; absent/"default" => canonical
nextSlugIndex=3                       ; new — monotonic slug counter

[compiler/cfg-1]
name=FBC 32bit                        ; required, user-facing display name
base=                                 ; empty/missing => "default"; else parent slug
path=                                 ; key present, value empty => override with empty string
compileCommand="<$fbc>" -arch x86 "<$file>"
; runCommand absent => inherit from base
; terminal absent => inherit from base

[compiler/cfg-2]
name=FBC 32bit GUI
base=cfg-1                            ; chain: cfg-2 → cfg-1 → default
compileCommand="<$fbc>" -arch x86 -s gui "<$file>"
```

Reserved keys inside `[compiler/*]` sections: `name`, `base`, `path`,
`runCommand`, `compileCommand`, `terminal`. Unknown keys are preserved by
ConfigManager but ignored.

## Data model

```cpp
struct ResolvedCompilerConfig {
    wxString              slug;            // "default" or "cfg-N"
    wxString              displayName;     // "Default" / user-entered name
    std::filesystem::path path;            // resolved through base chain
    wxString              runCommand;      // resolved (meta-tags unexpanded)
    wxString              compileCommand;
    wxString              terminal;
};

enum class CompilerField { Path, CompileCommand, RunCommand, Terminal };
```

`path` is `std::filesystem::path`; the three command/template fields stay as
`wxString` because they carry meta-tag templates the existing CompileCommand /
RunCommand expand.

## CompilerConfigCatalog API

`src/lib/compiler/CompilerConfigCatalog.{hpp,cpp}`. Owned by `CompilerManager`
as `std::unique_ptr`.

```cpp
class CompilerConfigCatalog {
public:
    explicit CompilerConfigCatalog(ConfigManager& cfg);

    void reload();                                              // re-parse [compiler] + [compiler/*]

    // Lookups (resolved through the base chain, with cycle/orphan fallback):
    const ResolvedCompilerConfig& canonical() const;            // slug == "default"
    const ResolvedCompilerConfig* find(wxStringView slug) const;
    std::vector<std::reference_wrapper<const ResolvedCompilerConfig>> all() const;
        // canonical first, then user-defined in numeric slug order

    // Active selector:
    wxString activeSlug() const;                                // "default" if unset or invalid
    void     setActiveSlug(wxStringView slug);                  // writes compiler.active; "default" clears the key

    // For Document → effective config (the rule above):
    const ResolvedCompilerConfig& resolveForDocument(const Document&) const;

    // CRUD (used by CompilerPage):
    wxString createFromCanonical(const wxString& displayName);  // allocates cfg-N, writes section, returns slug
    wxString copy(wxStringView sourceSlug,
                  const wxString& displayName);                 // deep copy of overrides + base
    void     remove(wxStringView slug);                         // canonical rejected; re-parents dependents
    void     rename(wxStringView slug, const wxString& newDisplayName);
    void     setBase(wxStringView slug, wxStringView newBaseSlug);
    void     setOverride(wxStringView slug,
                         CompilerField field,
                         std::optional<wxString> value);        // nullopt = remove key (inherit)

    // Slugs that may legally be a base for `slug`:
    std::vector<wxString> validBasesFor(wxStringView slug) const;
};
```

### Remove semantics

- Canonical rejected (UI hides the button; assertion in code).
- Any user-defined config that had the removed slug as `base` is re-parented
  to `default`.
- `compiler.active` cleared to `"default"` if it was the removed slug.
- Open `Document*` references walked; any with `m_configuration == removedSlug`
  → cleared to `{}`.
- Session files on disk are **not** walked; they self-heal on next load
  (catalog `find` returns nullptr, resolution falls back to active, next save
  writes the absent key).

## Document state

`src/lib/document/Document.hpp`:

```cpp
private:
    std::optional<wxString> m_configuration;       // empty => follow active

public:
    const std::optional<wxString>& configuration() const noexcept;
    void setConfiguration(std::optional<wxString>);
```

Plain getter/setter. The "matches active → empty" normalization lives in
`CompilerManager`, not here.

## Session format

`src/lib/document/FileSession.cpp`, v3 (no version bump — additive):

```ini
[file_007]
path=...
scroll=42
cursor=1304
encoding=UTF-8
eolMode=LF
configuration=cfg-2          ; new, optional; omitted when m_configuration is empty
```

Write: only when `m_configuration.has_value()`.
Read: store as-is. No validation at load — resolution is lazy.

Legacy v0.1 / v0.2 readers unchanged (those files never carry compiler config).

## CommandId + toolbar wiring

- New `CommandId::Configuration` in `CommandId` enum.
- Registered in `CommandManager::addCommands(...)` with `kind = wxITEM_DROPDOWN`.
  Naming convention follows existing commands.
- Locale string under `commands.configuration.name` and `commands.configuration.help`.
- Bitmap not required (combobox carries its own label) but provide a placeholder
  for menu rendering if needed.

`layout.toolbar` entry: insert the token `configuration` wherever the user
wants the combobox. No structural change to layout config — still comma-
separated string list.

`src/lib/ui/UIManager.cpp::configureToolBar`, replace the existing `AddTool`
branch with:

```cpp
if (entry->id == CommandId::Configuration && entry->kind == wxITEM_DROPDOWN) {
    auto* combo = m_ctx.getCompilerManager().createConfigurationCombo(m_auiToolbar.get());
    m_auiToolbar->AddControl(combo, name);
    // No bind chain — combobox is owned and managed by CompilerManager.
    continue;
}
m_auiToolbar->AddTool(entry->id, name, bitmap, help, entry->kind);
```

The combobox is **not** added to `entry->binds` — it isn't a tool and doesn't
need the enable/check plumbing in `CommandEntry::apply` (the variant visitor
at `src/lib/command/CommandEntry.cpp:56`).

## CompilerManager additions

```cpp
class CompilerManager {
    std::unique_ptr<CompilerConfigCatalog> m_catalog;
    wxComboBox*                            m_configCombo  = nullptr;  // non-owning; toolbar owns
    Document*                              m_lastActiveDoc = nullptr;

public:
    CompilerConfigCatalog&       catalog()       { return *m_catalog; }
    const CompilerConfigCatalog& catalog() const { return *m_catalog; }

    // Called by UIManager during toolbar construction.
    wxComboBox* createConfigurationCombo(wxAuiToolBar* parent);

    // Called by UIManager after settings dialog OK (catalog mutations).
    void refreshConfigurationCombo();

    // Called by UIManager from onPageChanged + after document open/create.
    void onActiveDocumentChanged(Document* doc);

    // Used by CompileCommand / RunCommand / quickRun / compileAndRun.
    const ResolvedCompilerConfig& resolveForActiveDocument() const;
};
```

### Combobox behavior

- Populates from `catalog().all()`; display strings are `displayName`,
  per-entry slug carried via `wxComboBox::SetClientData` (or a parallel
  `std::vector<wxString>` of slugs by index).
- `wxCB_READONLY` — user picks from the list only.
- Fixed width (140–180px) so it doesn't flex unpredictably in the toolbar.
- Disabled when active document isn't a FreeBASIC source (determined by
  `doc->type()`), or when no document is active.
- On `wxEVT_COMBOBOX`: look up slug of the selected entry, call the
  normalizing `setDocumentConfiguration(m_lastActiveDoc, slug)` (the
  "matches active → empty" rule).
- On `onActiveDocumentChanged(doc)`: set selection to
  `resolveForDocument(*doc).slug`.

## Active document change hook

`UIManager::onPageChanged` (`src/lib/ui/UIManager.cpp:40`) is the central
refresh hub — already updates window title, statusbar, toolbar button state,
sidebar. Add one call:

```cpp
m_ctx.getCompilerManager().onActiveDocumentChanged(activeDoc);
```

Programmatic activation via `DocumentManager::setActive` already routes
through `notebook->SetSelection` which fires `wxEVT_AUINOTEBOOK_PAGE_CHANGED`
on every platform (verify on macOS — wxAuiNotebook usually does, but
`ChangeSelection` would not). If the event isn't fired in the programmatic
path, add an explicit `onActiveDocumentChanged` call there too.

## CompileCommand / RunCommand integration

Refactor `src/lib/compiler/CompileCommand.cpp` and `RunCommand.cpp` to take
a `const ResolvedCompilerConfig&` instead of reading config directly:

```cpp
// Before
wxString CompileCommand::build(Context& ctx) {
    auto cmd = ctx.getConfigManager().config()
                  .at("compiler").get_or("compileCommand", DEFAULT);
    auto path = ctx.getConfigManager().config()
                  .at("compiler").get_or("path", "");
    ...
}

// After
wxString CompileCommand::build(const ResolvedCompilerConfig& cfg,
                                const Document& doc) {
    auto cmd  = cfg.compileCommand.IsEmpty() ? DEFAULT_COMPILE_COMMAND : cfg.compileCommand;
    auto path = cfg.path;
    ...
}
```

`CompilerManager::compile/compileAndRun/run/quickRun` each resolve once at
the start of the operation via `m_catalog->resolveForDocument(*doc)` and
thread the resolved config through `BuildTask`. `BuildTask` gains a
`ResolvedCompilerConfig` member captured at construction so the resolution
is stable for the duration of the build (user changing config mid-compile
doesn't partially apply).

Meta-tag expansion (`<$fbc>`, `<$file>`, etc.) is unchanged — same source
strings, same expansion, just sourced from `ResolvedCompilerConfig`.

## Settings dialog — `CompilerPage` rewrite

`src/lib/settings/panels/CompilerPage.{hpp,cpp}` replaced. Layout uses the
project's `vbox`/`hbox` helpers from `src/lib/ui/controls/Layout.hpp` — **no
`wxStaticBoxSizer`**.

```cpp
vbox(tr("dialogs.settings.compiler.configurations"), { .proportion = 1 }, [&] {
    hbox({ .proportion = 1, .margin = false }, [&] {
        // left: list + Add / Copy / Remove buttons
        vbox({ .proportion = 1, .margin = false }, [&] {
            add(m_configList);                         // wxListBox
            hbox({ .margin = false }, [&] {
                add(m_addButton);
                add(m_copyButton);
                add(m_removeButton);
            });
        });
        // right: name / slug / base / active + four InheritableFields
        vbox({ .proportion = 2, .margin = false }, [&] {
            // name + slug
            // base dropdown
            // "Active for new files" checkbox
            // InheritableField rows: path, compileCommand, runCommand, terminal
        });
    });
});

vbox(tr("dialogs.settings.compiler.help"), { .margin = false }, [&] {
    // CHM file picker row (relocated from current location)
});
```

### List rendering

- `wxListBox`, single-select.
- Canonical Default at row 0, locked (cannot be removed; name not editable).
- User-defined configs follow, indented by chain depth using leading spaces
  (no `wxTreeCtrl` — keeps it simple, no expand/collapse state).
- The active row is rendered **bold** with **" (active)"** suffix (per design).
  If no active is defined, Default is considered active and gets the marker.

### Buttons

- **Add** — always enabled. Creates a new config inheriting from Default with
  a placeholder name. Selects the new row, focuses the name field.
- **Copy** — enabled when a non-canonical row is selected. Deep-copies all
  overrides and the `base` field; allocates a new slug; opens with name
  pre-filled "<original> (copy)".
- **Remove** — enabled when a non-canonical row is selected. Confirms before
  removing. Re-parenting and active-clearing happen via catalog.

### Right pane

- **Name** — `wxTextCtrl`, disabled for canonical.
- **Slug** — read-only static text under the name (e.g. `Slug: cfg-1`).
- **Base** — `wxChoice` populated via `catalog.validBasesFor(slug)`. Hidden
  for canonical.
- **Active for new files** — `wxCheckBox`. Mutually exclusive: checking it on
  a row clears the active on others (via `catalog.setActiveSlug`). Unchecking
  the only active row sets active back to Default.
- **Four InheritableField rows** — Path (`Kind::Path`), CompileCommand
  (`Kind::Text`), RunCommand (`Kind::Text`), Terminal (`Kind::Text`).

### Apply / Cancel

- The page maintains a draft state (copy-on-edit) and commits to the catalog
  only on OK. Cancel discards.
- Apply: iterate fields per draft config, call
  `catalog.setOverride(slug, field, inherited ? nullopt : value)`; call
  `catalog.setActiveSlug` if active changed; call `ConfigManager::save(Category::Config)`.
- After apply, `SettingsDialog` calls `CompilerManager::refreshConfigurationCombo()`
  so the toolbar reflects the new state immediately.

### CHM help

`vbox(tr("dialogs.settings.compiler.help"), { .margin = false }, ...)` below
the configurations group. Single file picker, reads/writes whatever existing
config key the current CHM integration uses (behavior unchanged — relocation
only).

## InheritableField widget

`src/lib/settings/widgets/InheritableField.{hpp,cpp}`, modelled on
`ColorPicker`:

```cpp
class InheritableField : public wxPanel {
public:
    enum class Kind { Text, Path };                      // Path adds a Browse button

    InheritableField(wxWindow* parent, Kind kind, const wxString& label);

    void  setInherited(bool inherited);                  // toggles checkbox + enables/disables field
    bool  isInherited() const noexcept;

    void  setOverrideValue(const wxString&);             // value while checkbox is checked
    wxString overrideValue() const;

    void  setResolvedValue(const wxString&);             // shown (greyed) while inherited

    // Emits wxEVT_CHECKBOX on the override tickbox + wxEVT_TEXT on the field.
};
```

Behavior:

- Tickbox unchecked (inherited): field disabled, value shows the resolved
  value greyed (set the resolved value into the disabled `wxTextCtrl` so the
  user can see what they're inheriting).
- Tickbox checked (override): field enabled, value pre-populated with the
  current resolved value at the moment of ticking (so the user doesn't lose
  context).
- Browse button (Path only): `wxFileDialog`, writes result into the override
  field.

Internal layout uses raw sizers only if `ColorPicker` already does (local
consistency); page-level composition stays on `vbox`/`hbox`.

## Backward compatibility

- Existing `[compiler]` sections without any `[compiler/*]` siblings: catalog
  exposes only canonical Default. Toolbar combobox shows one entry. Behavior
  identical to today.
- Sessions without `configuration=` per file: load as `m_configuration = {}`.
  Follow active (defaults to canonical). Identical to today.
- `compiler.active` and `compiler.nextSlugIndex` absent: treated as `"default"`
  and `1` respectively.
- Downgrade safety: a user downgrading to a pre-feature build keeps working.
  `[compiler/*]` sections silently ignored. `compiler.active`/`nextSlugIndex`
  ignored. Session `configuration=` keys ignored.

## Tests

`tests/unit/`. No UI tests on macOS (per project convention).

- **`CompilerConfigCatalogTests`**:
  - Parse canonical-only config → one entry, slug `default`.
  - Parse canonical + `[compiler/cfg-1]` with overrides → resolved fields correct.
  - Inheritance chain: `cfg-2` (base `cfg-1`) inherits `cfg-1`'s overrides,
    falls through to canonical for un-overridden fields.
  - Field-missing vs field-present-empty: empty value is an override
    (resolved = empty); missing is inheritance.
  - `setOverride(..., nullopt)` removes the key.
  - Cycle detection: `setBase` rejects self-base and descendant-base.
  - Orphan / cycle on reload: resolves to canonical; warning emitted; entry
    remains in catalog.
  - `remove()` re-parents dependents to `default` and clears
    `compiler.active` if it pointed at the removed slug.
  - Slug allocation: `nextSlugIndex` increments correctly and persists.
- **`FileSession` round-trip extensions**:
  - Doc with `m_configuration = "cfg-2"` round-trips.
  - Absent → `{}`.
  - Missing-on-load (catalog empty): still loaded into `m_configuration`,
    validation deferred to first compile.
- **`CompilerManager::setDocumentConfiguration` normalization**:
  - Picking active → `{}`.
  - Picking non-active → slug.
  - Active flipping (without doc change): empty docs follow the new active.

## Open question (still need to confirm)

The "matches active → empty" rule applied when the canonical Default is
involved:

- `compiler.active = "default"`, user picks "Default" → `m_configuration = {}`.
- `compiler.active = "cfg-1"`, user picks "Default" → `m_configuration = "default"` (pinned).
- `compiler.active = "cfg-1"`, user picks "cfg-1" → `m_configuration = {}`.

Same UI action stores different state depending on active. This is the
intended semantic per #1 and #8, but worth explicit confirmation since it's
subtle.
