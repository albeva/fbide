# Theme Refactoring Plan

## Goal

Replace `ThemeOld` with the new `Theme` class everywhere the IDE
reads/writes theme data. End state: a single source of truth for
theme categories (syntax + extras) that drives `Theme`, `FBSciLexer`,
and the settings UI. Adding a category means touching one
`DEFINE_THEME_*` macro; everything else auto-adjusts.

---

## Consistency review of requested points

1. **Remove `Theme` from `ConfigManager::Category`.**
   Currently `ConfigManager` owns a generic `Value` tree per theme
   INI. The new `Theme` class already parses/serializes itself, so
   double bookkeeping is redundant. `Theme` should live on
   `ConfigManager` (not `Context`) as a plain member.

2. **`ConfigManager::getTheme() -> Theme&`.**
   Implies `Context::getTheme()` either delegates or is removed.
   Callers today: `Editor.cpp` (3×), `CompilerLog.cpp`,
   `FormatDialog.cpp`, `ThemePage.cpp`, `App.cpp`. All currently hit
   `ThemeOld`. They need two things concurrently:
   - switch from `ThemeOld` to `Theme` API;
   - switch from `ctx.getTheme()` to `ctx.getConfigManager().getTheme()` (or keep a thin `Context::getTheme()` forwarder for brevity — recommended).

3. **`Theme::load` dispatch on extension.**
   `.fbt` → `loadV4(path)` then set `m_themePath = path` (note: v4
   otherwise discards path; dispatch path must assign it so save can
   migrate). Any other extension → existing INI loader.

4. **Save migrates extension on old version.**
   If `m_version == Version::oldFbide()`, rewrite `m_themePath` from
   `.fbt` to `.ini` before writing. `save()` then bumps
   `m_version = Version::fbide()` so subsequent saves stay put.
   **Edge case:** if the `.ini` target already exists (e.g. user
   saved once, reloaded `.fbt`), overwrite is acceptable but worth a
   log line.

5. **`getPath()` + ConfigManager sync.**
   After successful save, `ConfigManager` updates `config["theme"]`
   to `relative(theme.getPath())` so the next launch picks up the
   migrated file. Config entry currently reads `theme=themes/classic.fbt`.

6. **Unified category enum for `ThemePage`.**
   Two macros feed it:
   - `DEFINE_THEME_CATEGORY` — syntax styles.
   - `DEFINE_THEME_EXTRA_CATEGORIES` — LineNumber, Selection, Brace, BadBrace.
   Generate:
   - unified `SettingsCategory` enum (or rename `ThemeCategory`
     + extras),
   - `operator+` for implicit int conversion,
   - `getSettingsCategoryName(cat) -> std::string_view` combining
     both,
   - `constexpr std::array kSettingsCategories` with full list.
   **Consistency concern:** `ThemeCategory` currently has `Default`
   (syntax). `Default` houses the editor-wide font/fontSize/default
   colours in the UI. So Default already plays the "editor defaults"
   role — no separate `Editor` category needed.

7. **Capability flags per category.**
   Syntax entries: fg, bg, bold, italic, underlined. Extras diverge:
   - LineNumber / Selection — colours only.
   - Brace / BadBrace — colours + bold/italic/underlined.
   - Default (syntax) — **only** category showing font + fontSize
     (editor-wide globals). Both controls are **hidden** (not just
     disabled) for every other category.
   Needed: a small descriptor table mapping category → supported
   controls; `font`/`fontSize` flag drives visibility.

8. **Locale handling.**
   Key naming: lowercase-first of category name
   (`LineNumber` → `lineNumber`). Fallback to
   `getThemeCategoryName(cat)` when locale lookup returns empty.
   English + Estonian are the only maintained locales — the other
   `.fbl` files live under `resources/IDE/lang/` (legacy, not v2).
   Delete outdated keys (`date`, `constant`, `asm`, `caret`,
   `stringClosed`, `stringOpen` variants that no longer map 1:1),
   add new keys (`default`, `multilineComment`, `string`, `stringOpen`,
   `keyword1..5`, `label`, `constant`, `preprocessor`, `error`,
   `lineNumber`, `selection`, `brace`, `badBrace`).

9. **Caret?** Caret colour is not in new `Theme` (old had
   `EditorStyle::caretColour`). Decision: drop caret editing in
   settings for now — it wasn't a distinct `ThemeCategory`. If we
   want it, extend `DEFINE_THEME_PROPERTY` with a `caret` `wxColour`
   property; out of scope for this refactor.

10. **Dead code.** After migration, `ThemeOld` + `ThemeTests.cpp`
    tests are orphaned. Keep `loadV4` (needs the old layout
    knowledge baked in), delete `ThemeOld.{hpp,cpp}` and related
    tests. Remove `Config::THEME_EXT` references (or switch default
    to `"ini"`).

11. **Bundled themes still `.fbt`.** `v2/themes/classic.ini`
    exists but is empty (23 bytes, placeholder); all real themes
    are `.fbt`. Ship path: config stays pointed at
    `themes/classic.fbt`; `Theme::load` dispatches to `loadV4` on
    first run; first user save writes a `.ini` under the writable
    config dir (not the resources folder). Do **not** flip
    `config_win.ini` default yet. Separate future task: bulk-migrate
    bundled `.fbt` → `.ini` via a one-off script or tool run.

---

## Phased plan

### Phase 1 — ConfigManager owns Theme
- Drop `Category::Theme` from `ConfigManager::Category` and
  `getCategoryName()`.
- Drop `theme()` accessor + `CAT_COUNT` adjust.
- Add `Theme m_theme;` + `getTheme() -> Theme&` /
  `getTheme() const -> const Theme&`.
- On construction (after `Config` loaded), read
  `config()["theme"]`, resolve via `absolute()`, call
  `m_theme.load(path)`.

### Phase 2 — Context delegation
- `Context::getTheme()` either:
  - (a) forward to `m_configManager->getTheme()`, or
  - (b) remove, update all call sites to go through
    `ctx.getConfigManager().getTheme()`.
- Pick (a) for minimal churn.
- Remove `std::unique_ptr<ThemeOld> m_theme` from `Context`.

### Phase 3 — Theme load/save smart dispatch
- In `Theme::load(path, reset)`:
  - if `path.Lower().EndsWith(".fbt")` → `loadV4(path);
    m_themePath = path;` and return.
  - else current INI path.
- In `Theme::save`:
  - before writing, if `m_version == Version::oldFbide()`, rewrite
    `m_themePath` extension `.fbt` → `.ini` (via `wxFileName`).
  - always stamp `m_version = Version::fbide()` so the saved file
    is current.
- Add `[[nodiscard]] auto getPath() const -> const wxString&;`.
- `ThemePage::apply`/`onSaveTheme`: after successful save, call
  `configManager.config()["theme"] = configManager.relative(theme.getPath())`
  and `configManager.save(Category::Config)`.

### Phase 4 — Unified category infrastructure
- In `ThemeCategory.hpp` (or a new `ThemeCategories.hpp`):
  - add `operator+(ThemeCategory)` — already present.
  - introduce a `SettingsCategory` enum derived from both
    `DEFINE_THEME_CATEGORY` and `DEFINE_THEME_EXTRA_CATEGORIES`
    (the latter from `Theme.hpp`) or move
    `DEFINE_THEME_EXTRA_CATEGORIES` into `ThemeCategory.hpp` so both
    live together.
  - `getSettingsCategoryName(cat) -> std::string_view` merged.
  - `kSettingsCategories` constexpr array, `kSettingsCategoryCount`.
- Add a capability descriptor:
  ```cpp
  struct SettingsCapability { bool fg, bg, font, fontSize, style; };
  constexpr auto capabilityOf(SettingsCategory) -> SettingsCapability;
  ```
  Default (syntax) = all; other syntax = fg/bg/style; LineNumber /
  Selection = fg/bg; Brace / BadBrace = fg/bg/style.

### Phase 5 — ThemePage UI rewrite
- Replace `ThemePage::Category` enum with `SettingsCategory`.
- Generate list-box entries by iterating `kSettingsCategories`:
  - lookup locale key `dialogs.settings.themes.categories.<lowerFirst(name)>`.
  - fall back to `getSettingsCategoryName(cat)` when locale tr empty.
  - **Default pinned at index 0**; remaining categories sorted
    alphabetically by display (translated) label. List index no
    longer matches enum order.
- **Index → category mapping.** Two workable options:
  1. `wxListBox::SetClientData(i, reinterpret_cast<void*>(static_cast<std::intptr_t>(+cat)))`
     on append; read back with `static_cast<SettingsCategory>(
     reinterpret_cast<std::intptr_t>(m_typeList->GetClientData(sel)))`.
     Pros: self-contained, no parallel state. Cons: pointer
     casting noise.
  2. Store a
     `std::array<SettingsCategory, kSettingsCategoryCount> m_categoryIndex;`
     on `ThemePage`, filled in the same order as list items. Size
     is known at compile time — no heap alloc, no resize. Pros:
     type-safe, trivially debuggable, fixed-size. Cons: extra field.
  Recommend **option 2** — cleaner with C++23 and keeps
  capability lookups off the list control. Either works; pick one
  convention and stick to it.
- Rebuild `loadCategory` / `saveCategory` to read capability
  descriptor and toggle control enablement.
- Hide font + fontSize controls unless capability has them (show =
  `Default` syntax category only).
- Remove `ThemeOld` field — use `Theme m_theme;`.
- Replace `toItemKind`/`isSyntaxStyle` helpers with a single
  capability lookup.
- Update save-new-theme flow to use `.ini` extension and new
  `Theme` API (`m_theme.save(path)`).

### Phase 6 — Locales
- `en.ini` + `et.ini` under `[dialogs/settings/themes/categories]`:
  - remove: `keywords1..4` (replace with `keyword1..5`),
    `stringClosed`, `stringOpen` (replace with new `string`,
    `stringOpen`), `date`, `asm`, `caret`, `textSelect`, `editor`,
    `braceMatch`, `braceMismatch`.
  - add: `default`, `comment`, `multilineComment`, `number`,
    `string`, `stringOpen`, `identifier`, `keyword1..5`, `operator`,
    `label`, `constant`, `preprocessor`, `error`, `lineNumber`,
    `selection`, `brace`, `badBrace`.
- Confirm `Context::tr` returns empty string on miss so fallback
  triggers.

### Phase 7 — Cleanup
- Delete `ThemeOld.hpp`, `ThemeOld.cpp`, drop from
  `src/lib/CMakeLists.txt`.
- Delete or rewrite `tests/ThemeTests.cpp` (new tests for `Theme`
  round-trip + `loadV4` migration).
- Flip default theme in `resources/IDE/v2/config_win.ini` from
  `themes/classic.fbt` to `themes/classic.ini`.
- Remove `Config::THEME_EXT` constant or switch to `"ini"` (it's
  still used by `ThemePage` paths via legacy `Config`).
- Ensure `Editor::applyStyle` consumes `Theme::Entry` +
  `Theme::getFont()`/`Theme::getFontSize()` directly — no per-style
  font anymore.

---

## Risks / open questions

- **Concurrent classes during refactor.** Phase 1–3 still compile
  with `ThemeOld` in `Context`; Phase 5 replaces it. Keeping both
  briefly is fine but double the getters avoid a naming clash.
- **Default theme migration on first run.** Shipping
  `classic.fbt` and auto-migrating means the `.fbt` in the install
  dir becomes stale after first save. Acceptable since we also ship
  `classic.ini`.
- **Hidden flag / letterCase** from v4 are already dropped by
  `loadV4`. Documented.
- **`Version::oldFbide()` returns 0.4.6.** Used as sentinel. New
  Theme sets `Version::fbide()` on save. Guard: invalid (0.0.0)
  Version must also trigger migration path to avoid silently
  persisting an empty version. Consider `isValid()` check.

---

## TODO (work order)

1. [ ] **Phase 1.a** — remove `Category::Theme` + `theme()` +
   switch arm from `ConfigManager`; bump `CAT_COUNT` down.
2. [ ] **Phase 1.b** — add `Theme m_theme;` +
   `getTheme()/getTheme() const` to `ConfigManager`; load in
   constructor from `config()["theme"]`.
3. [ ] **Phase 2** — forward `Context::getTheme()` to
   `ConfigManager::getTheme()`; drop `std::unique_ptr<ThemeOld>
   m_theme` from `Context`. Update `#include`s.
4. [ ] **Phase 3.a** — `Theme::load` dispatch on `.fbt`; call
   `loadV4` + set `m_themePath`.
5. [ ] **Phase 3.b** — `Theme::save` migrate `.fbt` → `.ini` path
   when legacy version; always stamp current version.
6. [ ] **Phase 3.c** — add `Theme::getPath()`.
7. [ ] **Phase 3.d** — callers update `config["theme"]` via
   `relative()` + save config after theme save.
8. [ ] **Phase 4.a** — move/share `DEFINE_THEME_EXTRA_CATEGORIES`
   so `ThemeCategory.hpp` can compose the unified settings list.
9. [ ] **Phase 4.b** — add `SettingsCategory` enum + `+operator`
   + name function + constexpr array.
10. [ ] **Phase 4.c** — add capability descriptor for UI.
11. [ ] **Phase 5.a** — `ThemePage`: swap `ThemeOld` for `Theme`;
    drive category list from array; use new theme getters.
12. [ ] **Phase 5.b** — `ThemePage`: lowercase-first locale lookup
    with fallback; auto-generate list entries with `Default` pinned
    first, remainder sorted alphabetically by translated label;
    maintain index → `SettingsCategory` mapping via
    `std::array<SettingsCategory, kSettingsCategoryCount>` (fixed
    size known at compile time) over `SetClientData`.
13. [ ] **Phase 5.c** — `ThemePage`: capability-driven enable /
    visibility. Font + fontSize controls are **hidden** (parent
    sizer must relayout via `Show(false)` + `Layout()`) for every
    category except `Default`; all other capability gaps map to
    `Enable(false)`.
14. [ ] **Phase 5.d** — `ThemePage::onSaveTheme`: ensure `.ini`
    extension + theme config entry update.
15. [ ] **Phase 6** — update `en.ini` + `et.ini` category keys.
16. [ ] **Phase 7.a** — delete `ThemeOld.{hpp,cpp}`; trim CMake.
17. [ ] **Phase 7.b** — rewrite `tests/ThemeTests.cpp` against new
    `Theme` (round-trip + `loadV4`).
18. [ ] **Phase 7.c** — leave `config_win.ini` default theme at
    `themes/classic.fbt` for now (bundled `.ini` files are empty);
    drop/repurpose `Config::THEME_EXT` (default to `"ini"` for new
    saves). Bulk migration of bundled themes is a separate future
    task.
19. [ ] **Phase 7.d** — `Editor::applyStyle` + `applyFreebasicTheme`
    use new `Theme` API only (single font/fontSize, ThemeCategory
    keys). Remove `ThemeOld` mentions from `Editor.cpp`,
    `CompilerLog.cpp`, `FormatDialog.cpp`.
20. [ ] **Verify** — build + run debug build, manual smoke test:
    open IDE with legacy `.fbt` → theme loads, save → migrates to
    `.ini`, restart → loads the new file, settings dialog shows all
    categories with correct control enablement.
