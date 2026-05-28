# Compiler Configurations — Phased TODO

Branch: `compiler-configurations`. Reference: [compiler-configurations.md](./compiler-configurations.md).

Each phase is independently buildable and testable. The app remains
functional at every phase boundary; user-visible behavior changes only in
phases 5 and 8.

---

## Phase 1 — Catalog foundation (parse + resolve only)

Pure data layer. Read existing config, expose canonical Default and any
user-defined sections via the catalog. **No behavior change** —
CompileCommand / RunCommand still read config directly in this phase.

- [ ] Create `src/lib/compiler/CompilerConfigCatalog.hpp`
  - [ ] `struct ResolvedCompilerConfig { slug, displayName, path (std::filesystem::path), runCommand, compileCommand, terminal }`
  - [ ] `enum class CompilerField { Path, CompileCommand, RunCommand, Terminal }`
  - [ ] `class CompilerConfigCatalog` with read-only API: `reload`, `canonical`, `find`, `all`, `activeSlug`, `resolveForDocument`
- [ ] Create `src/lib/compiler/CompilerConfigCatalog.cpp`
  - [ ] Parse `[compiler]` → canonical (slug = `"default"`)
  - [ ] Parse `[compiler/*]` → user configs in numeric slug order
  - [ ] Resolution walks base chain; missing key = inherit; present-but-empty = override
  - [ ] Cycle detection at load → fall back to canonical, `wxLogWarning` with slug + reason
  - [ ] Orphan base (unknown slug) → same fallback behavior
  - [ ] `activeSlug()` reads `compiler.active`; returns `"default"` if unset or invalid
- [ ] Wire into CMake (`src/lib/CMakeLists.txt`)
- [ ] Create `tests/unit/CompilerConfigCatalogTests.cpp`
  - [ ] Canonical-only config → one entry with slug `default`
  - [ ] Canonical + one user config with all overrides → resolved fields equal overrides
  - [ ] Chain resolution: `cfg-2` (base `cfg-1`) inherits from `cfg-1`, then canonical
  - [ ] Field absent vs field present-empty distinction
  - [ ] Cycle detected at load → resolves to canonical + warning
  - [ ] Orphan base at load → same as above
  - [ ] `activeSlug` defaults / invalid slug handling
- [ ] Add tests to `tests/CMakeLists.txt`

**Phase 1 acceptance:** unit tests pass; app builds and runs identically to
before; no integration with anything else yet.

---

## Phase 2 — Plumbing refactor (canonical-only)

Wire `CompilerManager` to own the catalog. Refactor `CompileCommand`,
`RunCommand`, `BuildTask` to take a `ResolvedCompilerConfig` instead of
reading config directly. Still uses only canonical Default end to end.

- [ ] `src/lib/compiler/CompilerManager.hpp`
  - [ ] Add `std::unique_ptr<CompilerConfigCatalog> m_catalog`
  - [ ] Add public `catalog()` accessor (const + non-const)
  - [ ] Add `resolveForActiveDocument()` returning const ref to canonical for now
- [ ] `src/lib/compiler/CompilerManager.cpp`
  - [ ] Construct `m_catalog` in ctor with `ConfigManager&`
  - [ ] All compile/compileAndRun/run/quickRun resolve once via catalog and pass to BuildTask
- [ ] `src/lib/compiler/CompileCommand.hpp/.cpp`
  - [ ] Change `build(Context&)` → `build(const ResolvedCompilerConfig&, const Document&)`
  - [ ] Read fields from the resolved config; meta-tag expansion unchanged
- [ ] `src/lib/compiler/RunCommand.hpp/.cpp`
  - [ ] Same refactor; reads `runCommand` / `terminal` from resolved config
- [ ] `src/lib/compiler/BuildTask.hpp/.cpp`
  - [ ] Capture `ResolvedCompilerConfig` at construction (so mid-build catalog
        changes don't half-apply)
  - [ ] Use captured config for all command construction
- [ ] Update existing tests
  - [ ] `tests/unit/CompileCommandTests.cpp` — adapt to new signature
  - [ ] `tests/unit/RunCommandTests.cpp` — same
  - [ ] `tests/unit/CompilerLogTests.cpp` if affected
- [ ] Hand-run a compile + run on a `.bas` file to confirm no regression

**Phase 2 acceptance:** all build/run paths work identically; existing tests
pass with updated signatures; no user-visible change.

---

## Phase 3 — Document state + session persistence

Per-document optional configuration slug. Persisted in session files. No UI
yet — only programmatic / test access.

- [ ] `src/lib/document/Document.hpp`
  - [ ] Add `std::optional<wxString> m_configuration`
  - [ ] Add `configuration()` getter, `setConfiguration(std::optional<wxString>)` setter
  - [ ] No side effects — Document remains a dumb holder
- [ ] `src/lib/document/FileSession.cpp`
  - [ ] On save (v3 writer): if `m_configuration.has_value()`, write
        `configuration=<slug>` under `[file_NNN]`; else omit
  - [ ] On load (v3 reader): read `configuration` if present, store as-is
        without validation
- [ ] Tests
  - [ ] `tests/unit/DocumentIOTests.cpp` — round-trip configuration:
        - [ ] Empty optional → key absent on disk
        - [ ] Slug → key present, round-trips
        - [ ] Missing-on-load with empty catalog → still loaded into optional
              (validation deferred to compile time)

**Phase 3 acceptance:** new field round-trips through session save/load;
Document API tests pass; behavior elsewhere unchanged (no compile path uses
the field yet).

---

## Phase 4 — CompilerManager: per-document resolution + normalization

Wire Document's optional slug into compile/run flow. Implement the
"matches active → empty" normalization helper. Still no UI.

- [ ] `src/lib/compiler/CompilerConfigCatalog`
  - [ ] Implement `resolveForDocument(const Document&)`:
        - [ ] If `doc.configuration()` empty → resolve `activeSlug()`
        - [ ] If has slug, exists → resolve slug
        - [ ] If has slug, missing → fall back to `activeSlug()` + warning
- [ ] `src/lib/compiler/CompilerManager`
  - [ ] Add `setDocumentConfiguration(Document&, wxStringView pickedSlug)`:
        - [ ] Normalizes: picked == active → `{}`, else slug
  - [ ] `resolveForActiveDocument()` now calls `m_catalog->resolveForDocument(*activeDoc)`
- [ ] Update compile/compileAndRun/run/quickRun to use `resolveForDocument`
      instead of canonical-only
- [ ] Tests
  - [ ] `CompilerConfigCatalogTests` — `resolveForDocument` scenarios
        (empty optional / valid slug / invalid slug / active changing)
  - [ ] `CompilerManagerTests` (new file if missing) — normalization rule:
        - [ ] Picking active when active is default → empty
        - [ ] Picking active when active is `cfg-1` → empty
        - [ ] Picking Default when active is `cfg-1` → stored as `"default"`
        - [ ] Active flipping after empty: subsequent resolve picks new active

**Phase 4 acceptance:** unit tests cover all branches; programmatic test of
setting a Document's slug results in correct config used for compile.

---

## Phase 5 — Toolbar combobox + active-doc hook

First user-visible feature: configuration combobox in the toolbar. Only
shows Default (CRUD not implemented yet). Switching tabs updates the
combobox; picking it updates the active document's stored optional.

- [ ] `src/lib/command/CommandId.hpp` — add `Configuration`
- [ ] `src/lib/command/CommandManager.cpp` — register `Configuration` with
      `kind = wxITEM_DROPDOWN`
- [ ] `resources/ide/locales/en.ini` (or wherever) — add
      `commands.configuration.name` ("Configuration") and `.help`
- [ ] `resources/ide/layout.ini` — insert `configuration` token in
      `toolbar=...` (sensible position, e.g. before `viewResult`)
- [ ] `src/lib/compiler/CompilerManager`
  - [ ] Add `m_configCombo` (non-owning `wxComboBox*`), `m_lastActiveDoc`
  - [ ] `createConfigurationCombo(wxAuiToolBar* parent)`:
        - [ ] Create `wxComboBox` parented to toolbar, `wxCB_READONLY`,
              fixed width
        - [ ] Populate from `catalog().all()`; per-entry slug via `SetClientObject`
              (or parallel vector)
        - [ ] Bind `wxEVT_COMBOBOX` → call normalization helper on `m_lastActiveDoc`
        - [ ] Return raw pointer (toolbar owns)
  - [ ] `refreshConfigurationCombo()` — repopulate, preserve selection
  - [ ] `onActiveDocumentChanged(Document* doc)`:
        - [ ] Update `m_lastActiveDoc`
        - [ ] Enable/disable based on FB-source-type check
        - [ ] Set selection to `catalog().resolveForDocument(*doc).slug`
- [ ] `src/lib/ui/UIManager.cpp::configureToolBar`
  - [ ] Special-case `CommandId::Configuration` + `wxITEM_DROPDOWN`:
        call `CompilerManager::createConfigurationCombo`, then
        `m_auiToolbar->AddControl(combo, name)`. Do **not** push to
        `entry->binds`.
- [ ] `src/lib/ui/UIManager.cpp::onPageChanged`
  - [ ] Add `m_ctx.getCompilerManager().onActiveDocumentChanged(activeDoc)`
- [ ] Verify on macOS that programmatic `DocumentManager::setActive` fires
      the page-changed handler. If it doesn't, add an explicit call there.
- [ ] Manual end-to-end check:
  - [ ] Combobox appears in toolbar with "Default" entry
  - [ ] Disabled for non-FB documents
  - [ ] Tab switch updates selection
  - [ ] Picking Default with active = default keeps `m_configuration` empty

**Phase 5 acceptance:** combobox visibly present and reactive. Default-only
end-to-end works. No CRUD yet (settings dialog still old-style).

---

## Phase 6 — Catalog CRUD

Mutating methods on the catalog: create / copy / remove / rename / setBase /
setOverride / setActiveSlug. Cycle prevention in `setBase`. Dependent
re-parenting in `remove`. Open-document walk in `remove`.

- [ ] `src/lib/compiler/CompilerConfigCatalog.hpp` — add CRUD method signatures
- [ ] `src/lib/compiler/CompilerConfigCatalog.cpp`
  - [ ] `createFromCanonical(displayName)`:
        - [ ] Allocate slug from `compiler.nextSlugIndex`, increment counter,
              persist
        - [ ] Write `[compiler/<slug>]` with only `name=` (no overrides yet)
        - [ ] Return assigned slug
  - [ ] `copy(sourceSlug, displayName)`:
        - [ ] Allocate new slug
        - [ ] Deep-copy all keys from source section
        - [ ] Overwrite `name=`
  - [ ] `remove(slug)`:
        - [ ] Reject canonical (assert)
        - [ ] Walk all `[compiler/*]` sections; re-parent any with
              `base == slug` to `"default"` (delete key)
        - [ ] Clear `compiler.active` if it equalled `slug`
        - [ ] Walk all open `Document*` via `DocumentManager`; clear their
              `m_configuration` if equal to `slug`
        - [ ] Remove the section
  - [ ] `rename(slug, newName)` — updates `name=` only
  - [ ] `setBase(slug, newBase)`:
        - [ ] Reject if `newBase == slug` or descendant of `slug`
        - [ ] Write/clear `base=` accordingly
  - [ ] `setOverride(slug, field, value)`:
        - [ ] `nullopt` → remove the key (inherit)
        - [ ] Otherwise write the value (including empty string)
  - [ ] `setActiveSlug(slug)`:
        - [ ] If `slug == "default"` → remove `compiler.active` key
        - [ ] Else write `compiler.active = slug`
  - [ ] `validBasesFor(slug)`:
        - [ ] Returns all slugs except `slug` and its transitive descendants
  - [ ] Every mutation triggers in-memory reload so `find`/`all`/`resolve`
        see the new state
- [ ] Catalog gains a `DocumentManager*` (or a callback) for the
      open-document walk on `remove`. Keep the dependency minimal.
- [ ] Tests
  - [ ] `nextSlugIndex` increments and persists
  - [ ] `createFromCanonical` assigns sequential slugs
  - [ ] `copy` deep-copies overrides and base
  - [ ] `remove` re-parents dependents to `default`
  - [ ] `remove` clears `compiler.active` if it matched
  - [ ] `remove` clears open-document `m_configuration` if it matched
  - [ ] `setBase` rejects self
  - [ ] `setBase` rejects descendant
  - [ ] `setOverride(nullopt)` removes the key (verify config Value tree)
  - [ ] `setOverride("")` writes empty string (key present)
  - [ ] `setActiveSlug("default")` removes `compiler.active` key
  - [ ] `validBasesFor` excludes self + descendants

**Phase 6 acceptance:** programmatically (via tests) the catalog supports
full lifecycle; persistence through `ConfigManager` confirmed.

---

## Phase 7 — `InheritableField` widget

Reusable widget for inheritable text and path fields. Modelled on
`ColorPicker`. Self-contained, no compiler coupling.

- [ ] Read `src/lib/settings/widgets/ColorPicker.{hpp,cpp}` (or wherever
      ColorPicker lives) to mirror the override-checkbox pattern
- [ ] Create `src/lib/settings/widgets/InheritableField.hpp`
  - [ ] `enum class Kind { Text, Path }`
  - [ ] Constructor `(wxWindow* parent, Kind kind, const wxString& label)`
  - [ ] `setInherited(bool)` / `isInherited()`
  - [ ] `setOverrideValue(wxString)` / `overrideValue()`
  - [ ] `setResolvedValue(wxString)` — for greyed display while inherited
- [ ] Create `src/lib/settings/widgets/InheritableField.cpp`
  - [ ] Layout: `[checkbox] [label] [text field] [Browse]?`
  - [ ] Tickbox unchecked → field disabled, shows resolved value greyed
  - [ ] Tickbox checked → field enabled, pre-populated with current resolved
        value (so the user sees a starting point)
  - [ ] Path: Browse button opens `wxFileDialog`, writes result to field
  - [ ] Emit `wxEVT_CHECKBOX` from tickbox; `wxEVT_TEXT` from field; both
        bubble to parent
- [ ] Wire into CMake

**Phase 7 acceptance:** widget compiles and is instantiable. No automated UI
tests (per project convention on macOS); manual visual check by dropping it
into a throwaway dialog if useful.

---

## Phase 8 — `CompilerPage` rewrite

Replace the existing CompilerPage with the list + editor layout. Wire to
catalog CRUD. CHM section relocated below configurations. Layout uses
`vbox`/`hbox` from `src/lib/ui/controls/Layout.hpp` — **no
`wxStaticBoxSizer`**.

- [ ] `src/lib/settings/panels/CompilerPage.hpp`
  - [ ] Drop existing single-config fields
  - [ ] Add draft-state members: copy of catalog snapshot + per-config
        edit state
  - [ ] List + buttons + name + slug + base + active-checkbox + four
        `InheritableField` members
- [ ] `src/lib/settings/panels/CompilerPage.cpp`
  - [ ] Layout via `vbox(tr("dialogs.settings.compiler.configurations"), ...)`
        wrapping `hbox` of left list + right editor
  - [ ] Left: `wxListBox` (single-select); rows = canonical + user configs;
        active row rendered bold with `" (active)"` suffix
  - [ ] Below list: `[Add]` / `[Copy]` / `[Remove]` buttons
  - [ ] Right: name `wxTextCtrl`, read-only slug static text, base `wxChoice`
        (populated via `validBasesFor`, hidden for canonical), active
        `wxCheckBox`, four `InheritableField` rows
  - [ ] Bottom: `vbox(tr("dialogs.settings.compiler.help"), ...)` containing
        the relocated CHM file picker
  - [ ] Selection change: load selected config into right pane
  - [ ] Edit handlers: update draft state, not catalog directly
  - [ ] `Add` → allocates a new draft inheriting from Default
  - [ ] `Copy` → deep-copies selected draft, new slug allocated
  - [ ] `Remove` → confirm prompt; removes draft; auto-re-parent dependents
        in the draft view
  - [ ] `apply()` (on OK):
        - [ ] For each draft config: catalog setOverride/setBase/rename
        - [ ] `setActiveSlug` if active changed
        - [ ] `ConfigManager::save(Category::Config)`
        - [ ] Tell `CompilerManager::refreshConfigurationCombo()`
  - [ ] Cancel: discard drafts (default `wxDialog` behavior; no extra code)
- [ ] `resources/ide/locales/en.ini` — add new strings:
  - [ ] `dialogs.settings.compiler.configurations`
  - [ ] `dialogs.settings.compiler.help`
  - [ ] `dialogs.settings.compiler.name`
  - [ ] `dialogs.settings.compiler.slug`
  - [ ] `dialogs.settings.compiler.base`
  - [ ] `dialogs.settings.compiler.activeForNewFiles`
  - [ ] `dialogs.settings.compiler.path`
  - [ ] `dialogs.settings.compiler.compileCommand`
  - [ ] `dialogs.settings.compiler.runCommand`
  - [ ] `dialogs.settings.compiler.terminal`
  - [ ] `dialogs.settings.compiler.add` / `.copy` / `.remove`
- [ ] Manual end-to-end:
  - [ ] Add a new config "FBC 64bit"; assert it appears in toolbar combobox
        after OK
  - [ ] Set it active; pick "Default" on a doc — stored slug should be
        `"default"`, not empty
  - [ ] Restart app, reopen session — pinned slug survives
  - [ ] Edit overrides via the InheritableField checkboxes; verify
        inheritance behavior at compile time
  - [ ] Remove a config that is base for another; child re-parents to Default
  - [ ] Remove a config that's set on an open doc; doc's stored slug clears

**Phase 8 acceptance:** end-to-end user-facing feature works. All TODOs
above complete. CHM picker functional in its new location.

---

## Phase 9 — Polish + cleanup

- [ ] Audit other locales for the new strings (or document that translations
      are missing as expected)
- [ ] Update `change-log.md` with a brief feature note
- [ ] Confirm `docs/compiler-configurations.md` matches what shipped; fix
      any drift
- [ ] Walk the open question (see end of spec) — confirm the
      "matches active → empty" semantic is what the user wants when active
      is non-canonical
- [ ] Run `tests/tests` end-to-end one more time
- [ ] Manual smoke test of compile / compileAndRun / run / quickRun on a
      `.bas` file with the active config and with a pinned non-default
      config
- [ ] PR description summarising the feature with screenshots of the
      settings page and toolbar

**Phase 9 acceptance:** feature ready for review/merge.

---

## Cross-cutting concerns (apply at all phases)

- [ ] Never run `ui-tests` on macOS (per project memory).
- [ ] No `wxStaticBoxSizer`; always `vbox(title, ...)` / `hbox(...)` from
      `src/lib/ui/controls/Layout.hpp`.
- [ ] `std::filesystem::path` for paths in `ResolvedCompilerConfig`.
- [ ] No new global state; everything threaded through `CompilerManager` /
      `ConfigManager`.
- [ ] Hand-test compile + run after every phase that touches the
      compile/run pipeline (phases 2, 4, 5, 6, 8).
