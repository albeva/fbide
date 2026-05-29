# Multiple Compiler Configurations — Design

Status: shipped on `compiler-configurations`.

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
  may override any field. Unspecified fields fall through to canonical
  Default. There is no chained inheritance — every user configuration
  inherits from Default and only from Default.
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

The "matches active → empty" normalization is enforced by the catalog when
the user picks a value in the toolbar combobox:

```cpp
const auto stored = catalog.normalizeForStorage(pickedSlug);
doc.setConfiguration(stored);  // nullopt when pickedSlug == active, else the slug
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

Single level only — user configuration → canonical Default. No chained
parents. A user wanting "GUI variant of my 32-bit config" copies the source
configuration (deep-copy of all overrides) and tweaks. Copying is cheap and
keeps mental models simple.

Any legacy `base=` key still present from earlier revisions of this feature
is silently ignored on load. It will not be persisted on the next save.

## Override semantics (per field)

- Key **absent** under `[compiler/<slug>]` → inherit from canonical.
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
path=                                 ; key present, value empty => override with empty string
compileCommand="<$fbc>" -arch x86 "<$file>"
; runCommand absent => inherit from canonical
; terminal absent => inherit from canonical

[compiler/cfg-2]
name=FBC 32bit GUI
compileCommand="<$fbc>" -arch x86 -s gui "<$file>"
```

Reserved keys inside `[compiler/*]` sections: `name`, `path`, `runCommand`,
`compileCommand`, `terminal`. Unknown keys are preserved by ConfigManager but
ignored.

## Data model

```cpp
struct ResolvedCompilerConfig {
    wxString              slug;            // "default" or "cfg-N"
    wxString              displayName;     // "Default" / user-entered name
    std::filesystem::path path;            // resolved against canonical
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

    // Lookups (each user config resolved against canonical):
    const ResolvedCompilerConfig& canonical() const;            // slug == "default"
    const ResolvedCompilerConfig* find(wxStringView slug) const;
    std::span<const ResolvedCompilerConfig> all() const;
        // canonical first, then user-defined in numeric slug order

    // Active selector:
    wxString activeSlug() const;                                // "default" if unset or invalid
    void     setActiveSlug(wxStringView slug);                  // writes compiler.active; "default" clears the key

    // For Document → effective config (the rule above):
    const ResolvedCompilerConfig& resolveByPinnedSlug(const std::optional<wxString>& pinnedSlug) const;
    std::optional<wxString>       normalizeForStorage(wxStringView pickedSlug) const;

    // CRUD (used by CompilerPage):
    wxString createFromCanonical(const wxString& displayName);  // allocates cfg-N, writes section, returns slug
    wxString copy(wxStringView sourceSlug,
                  const wxString& displayName);                 // deep copy of overrides
    bool     remove(wxStringView slug);                         // canonical rejected; clears active if matched
    bool     rename(wxStringView slug, const wxString& newDisplayName);
    bool     setOverride(wxStringView slug,
                         CompilerField field,
                         const std::optional<wxString>& value); // nullopt = remove key (inherit)
};
```

### Remove semantics

- Canonical rejected (UI hides the button; warning logged in code).
- `compiler.active` cleared to `"default"` if it was the removed slug.
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
the catalog (`normalizeForStorage`), not here.

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

- `CommandId::Configuration` registered with `kind = wxITEM_DROPDOWN`.
- Locale strings under `commands.configuration.name` and `commands.configuration.help`.
- `layout.toolbar` entry inserts the `configuration` token wherever the user
  wants the combobox.
- `UIManager::configureToolBar` special-cases the `Configuration` command to
  call `CompilerManager::createConfigurationCombo` instead of `AddTool`. The
  combobox is not added to `entry->binds`.

## CompilerManager additions

```cpp
class CompilerManager {
    std::unique_ptr<CompilerConfigCatalog> m_catalog;
    wxComboBox*                            m_configCombo  = nullptr;  // non-owning; toolbar owns
    Document*                              m_lastActiveDoc = nullptr;

public:
    CompilerConfigCatalog&       catalog()       { return *m_catalog; }
    const CompilerConfigCatalog& catalog() const { return *m_catalog; }

    wxComboBox* createConfigurationCombo(wxAuiToolBar* parent);
    void refreshConfigurationCombo();
    void onActiveDocumentChanged(Document* doc);

    void setDocumentConfiguration(Document& doc, const wxString& pickedSlug);
};
```

### Combobox behavior

- Populates from `catalog().all()`; display strings are `displayName`,
  per-entry slug carried in a parallel `std::vector<wxString>` indexed by row.
- `wxCB_READONLY` — user picks from the list only.
- Fixed width so it doesn't flex unpredictably in the toolbar.
- Disabled when active document isn't a FreeBASIC source, or when no
  document is active.
- On `wxEVT_COMBOBOX`: look up slug of the selected entry, call
  `setDocumentConfiguration` (normalises via `catalog.normalizeForStorage`).
- On `onActiveDocumentChanged(doc)`: set selection to
  `catalog().resolveByPinnedSlug(doc->configuration()).slug`.

## CompileCommand / RunCommand integration

`CompileCommand::build` / `RunCommand::build` take a `const
ResolvedCompilerConfig&` instead of reading config directly. `CompilerManager`
resolves once at the start of each operation via
`catalog().resolveByPinnedSlug(doc->configuration())` and threads the
resolved config through `BuildTask`. `BuildTask` captures the
`ResolvedCompilerConfig` at construction so the resolution is stable for the
duration of the build (user changing config mid-compile doesn't partially
apply).

Meta-tag expansion (`<$fbc>`, `<$file>`, etc.) is unchanged — same source
strings, same expansion, just sourced from `ResolvedCompilerConfig`.

## Settings dialog — `CompilerPage`

`src/lib/settings/panels/CompilerPage.{hpp,cpp}`. Layout uses the project's
`vbox`/`hbox` helpers from `src/lib/ui/controls/Layout.hpp` — **no
`wxStaticBoxSizer`**.

Left: `wxListBox` (single-select). Canonical Default at row 0; user
configurations follow in numeric slug order. The active row carries a
localised " (active)" suffix.

Below the list: Add / Copy / Remove icon buttons. Copy is enabled when any
row is selected; Remove is disabled for canonical.

Right: name `wxTextCtrl`, read-only slug static text, "Active for new files"
`wxCheckBox`, four `InheritableField` rows — Path (`Kind::Path`),
CompileCommand, RunCommand, Terminal (each `Kind::Text`).

Canonical Default has nothing to inherit from, so its right pane hides the
name field and renders the four field rows without an inherit checkbox
(plain editable inputs).

### Apply / Cancel

The page snapshots `[compiler]` on `create()` and restores it on `cancel()`
— that's how every CRUD action is rolled back atomically. `apply()`
validates that no user configuration has an empty `name=`, then calls
`CompilerManager::refreshConfigurationCombo()` so the toolbar reflects the
new state.

### CHM help

Below the configurations group, a `vbox(tr("help"), …)` contains the CHM
file picker. Behaviour unchanged — relocation only.

## InheritableField widget

`src/lib/ui/controls/InheritableField.{hpp,cpp}`, modelled on `ColorPicker`.

```cpp
class InheritableField : public Layout<wxPanel> {
public:
    enum class Kind { Text, Path };                      // Path adds a Browse button

    InheritableField(wxWindow* parent, Kind kind, wxString labelText, wxString inheritTooltip = {});
    void  create();

    void  setInherited(bool inherited);                  // toggles checkbox + enables/disables field
    bool  isInherited() const noexcept;

    void  setOverrideValue(const wxString&);             // value while checkbox is checked
    wxString overrideValue() const;

    void  setResolvedValue(const wxString&);             // shown (greyed) while inherited

    void  setInheritCheckboxVisible(bool visible);       // canonical Default hides the checkbox
};
```

Behavior:

- Tickbox unchecked (inherited): field disabled, value shows the resolved
  (canonical) value greyed so the user can see what they're inheriting.
- Tickbox checked (override): field enabled, value pre-populated with the
  current resolved value at the moment of ticking.
- Browse button (Path only): `wxFileDialog`, writes result into the override
  field.
- Hidden checkbox (`setInheritCheckboxVisible(false)`): field behaves as a
  plain editable input, used for canonical Default.

## Backward compatibility

- Existing `[compiler]` sections without any `[compiler/*]` siblings: catalog
  exposes only canonical Default. Toolbar combobox shows one entry. Behavior
  identical to today.
- Sessions without `configuration=` per file: load as `m_configuration = {}`.
  Follow active (defaults to canonical). Identical to today.
- `compiler.active` and `compiler.nextSlugIndex` absent: treated as `"default"`
  and `1` respectively.
- `base=` keys persisted by earlier revisions of this feature are ignored.
  The catalog treats every user config as inheriting directly from canonical.
- Downgrade safety: a user downgrading to a pre-feature build keeps working.
  `[compiler/*]` sections silently ignored. `compiler.active`/`nextSlugIndex`
  ignored. Session `configuration=` keys ignored.

## Tests

`tests/unit/CompilerConfigCatalogTests.cpp`. No UI tests on macOS (per
project convention).

- Canonical-only config → one entry, slug `default`.
- Canonical + `[compiler/cfg-1]` with overrides → resolved fields correct,
  unspecified fields fall through to canonical.
- Field-missing vs field-present-empty: empty value is an override
  (resolved = empty); missing is inheritance.
- Default templates applied when canonical key is absent.
- `setOverride(..., nullopt)` removes the key.
- `activeSlug` defaults / valid slug / invalid slug handling.
- `resolveByPinnedSlug` — empty optional follows active; explicit slug pins;
  missing pinned falls back to active.
- `normalizeForStorage` — collapses to nullopt when picked matches active.
- CRUD: `createFromCanonical` allocates sequential slugs, never reused after
  remove; `copy` duplicates overrides; `remove` clears `compiler.active` if
  it pointed at the removed slug; `setActiveSlug("default")` clears the key.
- `all()` ordering: canonical first, then user configs by numeric slug.
