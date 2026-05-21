# Layered Config Plan

Branch: `feature/layered-config`

## Problem

The current `READONLY` sentinel triggers a one-shot photocopy of the bundle's IDE resources to `<UserDataDir>/ide/` via `copyMissingResources` (`src/lib/config/ConfigManager.cpp:52`). The copy only fills *missing* files, never refreshes existing ones. Consequence: any bundle update that changes a key in an already-mirrored file never reaches the user. The shipped defaults silently rot.

## Approach: per-category overlay (`.local.ini`)

For the four mutable INI categories — `config`, `keywords`, `shortcuts`, `layout` — introduce a `<base>.local.ini` overlay file holding only keys whose value differs from the bundle baseline. At load: parse bundle → parse overlay → deep-merge overlay over baseline. At save: diff merged tree against the retained baseline, persist only divergent leaves; empty overlay → delete file.

- Overlay structure mirrors the base file 1:1. No special sections, no add/remove markers, no metadata.
- Key-presence in overlay = override at that path, regardless of value (empty values are still overrides).
- `locale` stays bundle-only — never copied, never overlaid. Custom locales handled by setting `locale=<path>` in `config_<plat>.local.ini`.
- Themes get a parallel but file-granular treatment (see Theme handling below).

The `READONLY` sentinel is reduced to a single routing flag: it only decides *where* writable files live (`<UserDataDir>` vs next to bundle). The mirror+copy mechanism is deleted entirely.

## Storage matrix

| category | bundle | overlay (default boot) | overlay location, READONLY | overlay location, portable | under `--config=PATH` |
|---|---|---|---|---|---|
| config | `<ideDir>/config_<plat>.ini` | yes | `<UserDataDir>/config_<plat>.local.ini` | `<ideDir>/config_<plat>.local.ini` | direct — no overlay |
| keywords | `<ideDir>/keywords.ini` | yes | `<UserDataDir>/keywords.local.ini` | `<ideDir>/keywords.local.ini` | direct |
| shortcuts | `<ideDir>/shortcuts_<plat>.ini` | yes | `<UserDataDir>/shortcuts_<plat>.local.ini` | `<ideDir>/shortcuts_<plat>.local.ini` | direct |
| layout | `<ideDir>/layout.ini` | yes | `<UserDataDir>/layout.local.ini` | `<ideDir>/layout.local.ini` | direct |
| locale | `<ideDir>/locales/<lang>.ini` | no — bundle-only | n/a | n/a | n/a |
| themes | `<ideDir>/themes/*.ini` (+ `.fbt` legacy) | full files, no overlay | user dir `<UserDataDir>/themes/`, copy-on-modify | edit in place in bundle dir | unchanged |

## `ConfigStrategy`

Internal value type encapsulating per-category storage policy. Lives entirely inside the config layer; `App.cpp` does not see it.

```cpp
class ConfigStrategy final {
public:
    [[nodiscard]] static auto overlay(wxString basePath, wxString overlayPath) -> ConfigStrategy;
    [[nodiscard]] static auto direct(wxString path) -> ConfigStrategy;

    [[nodiscard]] auto basePath()    const -> const wxString&;
    [[nodiscard]] auto overlayPath() const -> const wxString&;   // empty when direct
    [[nodiscard]] auto savePath()    const -> const wxString&;   // overlay if Overlay, base if Direct
    [[nodiscard]] auto usesOverlay() const -> bool;

private:
    enum class Mode : std::uint8_t { Overlay, Direct };
    Mode     m_mode { Mode::Direct };
    wxString m_basePath {};
    wxString m_overlayPath {};
};
```

`Entry` carries one. Built inside the `ConfigManager` ctor using existing bootstrap inputs (`appPath`, `idePath`, `configPath`) plus the sentinel result. The public `ConfigManager` constructor signature is unchanged.

## Strategy selection rules

Inside the `ConfigManager` ctor, after `m_appDir` / `m_ideDir` / READONLY detection:

- **`--config=PATH` passed** → `Config` category gets `ConfigStrategy::direct(PATH)`. The other three mutable categories *also* get `direct(...)` resolved against the paths the explicit config names. No overlays anywhere in this mode — reproducibility for CI / repro / `fbide --config=test.ini`.
- **Default boot** → every mutable category gets `ConfigStrategy::overlay(basePath, overlayPath)`:
  - `basePath` — bundle file resolved against `m_ideDir`.
  - `overlayPath` — `<UserDataDir>/<base>.local.ini` if READONLY sentinel present, else `<ideDir>/<base>.local.ini`.

`reloadConfig(path)` and `setCategoryPath(cat, path)` rebuild a fresh `ConfigStrategy` for the affected category using the same rules (the ctor retains `appDir` / `ideDir` / READONLY flag for this).

## Load / save behaviour

### `load(category)`

1. Parse `strategy.basePath()` via `wxFileConfig` → baseline `Value` tree (same as today's `importGroup`, `ConfigManager.cpp:111`).
2. If `strategy.usesOverlay()` and the overlay file exists → parse it the same way, then deep-merge overlay leaves over baseline.
3. Store **both** baseline and merged tree on `Entry`. Baseline is needed for save-time diff.
4. Existing missing-file fatality rules apply only to the baseline (`Config` / `Layout` / `Locale` fatal; `Keywords` / `Shortcuts` silent-empty). Missing overlay is always fine.

### `save(category)`

- **`Direct` strategy** → write merged tree to `savePath()`. Existing `exportGroup` against the pre-parsed `wxFileConfig` (same read-before-write trick on `ConfigManager.cpp:478` to preserve comments / ordering).
- **`Overlay` strategy** → diff merged tree against retained baseline. Collect every leaf path whose effective value differs (presence-with-different-value *or* presence-where-baseline-has-no-key). Write that subset to `overlayPath()`. If the diff is empty, delete the overlay file (or skip writing if it doesn't exist). Read the existing overlay file first to preserve comments / ordering for hand-edited overlays.

Keywords use the same logic: any leaf present in the overlay overrides the same leaf in baseline, regardless of value. No special add/remove sections.

### `reloadIfKnown(path)`

Match `path` against `strategy.basePath()` *or* `strategy.overlayPath()` for each category — either triggers a reload of that category (and the cascade if it's `Config`). Theme path matching unchanged.

## Migration of legacy mirrored files

**Skipped.** FBIde has only shipped in beta / rc to date, so we don't carry forward the legacy full-copy `<base>.ini` files that the old mirror produced in `<UserDataDir>`. Existing pre-release users with a stale mirror will see fresh empty overlays on first launch with the new code; if they want their prior customisations they re-apply them by hand. Old per-user theme copies under `<UserDataDir>/ide/themes/` are still picked up by the new two-dir theme enumeration — they just won't migrate to a different layout.

## Theme handling

Two writable mechanisms depending on READONLY:

- **READONLY present** — bundle themes immutable in `<ideDir>/themes/`. User themes live in `<UserDataDir>/themes/`. `getAllThemes()` enumerates both dirs, user dir wins on basename collision. Editing a bundle-owned theme triggers copy-on-modify: duplicate file to user themes dir, repoint `config["theme"]`, then open editor. Default duplicate name: `<original>-copy.ini`.
- **READONLY absent** — bundle themes dir is itself writable. Edits go in place. New themes saved into the same `<ideDir>/themes/` dir. No two-dir enumeration needed (or equivalently: bundle dir == user dir).

`absolute()` for `config["theme"]` resolution: prefer `<UserDataDir>/themes/<rel>` first when READONLY, then existing fallback chain (`ideDir`, `appDir`, `cwd`). Stored value remains a relative path like `themes/dark.ini`.

`Theme::save()` always targets the writable themes dir for the current mode. `.fbt` legacy extension stays readable via the existing `enumerate` glob (`ConfigManager.cpp:179`).

## Files touched

- `src/lib/config/ConfigStrategy.hpp` (new) — value type definition + factories.
- `src/lib/config/ConfigStrategy.cpp` (new, tiny) — out-of-line factories if needed.
- `src/lib/config/ConfigManager.hpp` — `Entry` gains `ConfigStrategy strategy` and baseline `Value`; small accessor additions for theme dirs.
- `src/lib/config/ConfigManager.cpp`:
  - Bootstrap rewritten to build `ConfigStrategy` per category.
  - `load()` / `save()` rewritten around strategy.
  - New helpers: `mergeOverlay(baseline, overlay)`, `diffAgainstBaseline(merged, baseline)`, `migrateLegacyMirror(category)`.
  - `copyMissingResources` deleted; `hasReadOnlySentinel` kept (reduced role).
  - Theme `absolute()` resolution gets user-themes-dir preference.
- Theme call sites — wire copy-on-modify trigger (locate during stage 8).
- `tests/unit/ConfigManagerTests.cpp` — new coverage (see Test surface).

`App.cpp`, `Context.{hpp,cpp}`, and every existing consumer of `ConfigManager`'s public API (`config()` / `save()` / `reloadIfKnown()` / etc.) are untouched.

## Test surface

`tests/unit/ConfigManagerTests.cpp` currently has no READONLY / mirror / migration coverage. New cases:

- **Overlay merge** — baseline + overlay → merged tree; overlay leaf replaces baseline leaf at same path, regardless of value (including empty string).
- **Aggressive prune on save** — saving a value that matches baseline → key absent from overlay; saving a divergent value → key present; saving with no diffs → overlay file deleted.
- **Strategy selection** — default boot vs `--config=PATH` build the right strategy per category; READONLY vs portable route overlay path to the correct dir.
- **Migration** — legacy `<base>.ini` present, no `.local.ini` → migrated, trimmed `.local.ini` written, legacy deleted; legacy present with existing `.local.ini` → no-op, legacy untouched.
- **Reload** — `reloadIfKnown(overlayPath)` triggers reload; `reloadIfKnown(basePath)` triggers reload; `Config` cascade still fires.
- **Hand-edited overlay preserved** — round-trip a manually written overlay with comments through `load` → `save` without losing them.

Themes: extend existing tests with a copy-on-modify case once stage 8 lands.

## Implementation order

Each stage should pass `verify` before the next begins.

1. **Introduce `ConfigStrategy`** (header + small `.cpp`). No callers yet. Trivial unit test that factories produce expected accessors.
2. **Add baseline `Value` to `Entry`.** Refactor `load()` to populate it. Still no overlay merging — behaviour unchanged.
3. **Overlay merge in `load()`** behind a single `usesOverlay()` check. Overlay paths still empty at this point → no behavioural change in production code; new merge helper unit-tested in isolation.
4. **Switch ctor to build `ConfigStrategy`** per category from the new rules. Overlay paths now populated. Default boot + `--config=PATH` both wired. Overlay files now load if present but no save logic yet → safe to ship behind a single commit.
5. **Rewrite `save()`** around strategy — diff + prune for overlay; direct write for direct. After this commit overlays are produced and consumed end-to-end.
6. **Legacy-mirror migration step** in ctor (READONLY only). Test thoroughly — touches user-dir files destructively.
7. **Delete `copyMissingResources`** and the READONLY mirroring branch. Reduce `hasReadOnlySentinel` to the routing role.
8. **Theme copy-on-modify** — locate the theme-edit entry point, add the duplicate-then-repoint flow, add `<UserDataDir>/themes/` to enumeration + `absolute()` precedence.
9. **Test additions** alongside each stage.

## Deferred to implementation time

- Exact location of the theme copy-on-modify trigger (which class / menu action) — quick grep when stage 8 lands.
- Whether existing `Theme::save()` supports targeting an arbitrary path or needs a small addition.
- Naming of `tests/data/` fixtures for migration / overlay scenarios.

## Out of scope

- UI for browsing / managing overlays (no "Default vs User Settings" diff view like VS Code).
- Versioning / migration of overlay schema across breaking config-key renames (handled ad-hoc if it comes up).
- Auto-import of dropped `.ini` theme files (drag-and-drop import) — possible future ergonomics.

## Implementation tasks

TDD where the unit is a pure function over data (helpers); refactor / integration-style where TDD wouldn't earn its keep.

1. **Survey ConfigManagerTests** — inventory existing coverage; identify test seams (likely need an optional `userDataDirOverride` ctor param). Research only.
2. **TDD: `ConfigStrategy` value type** — tests for `overlay()` / `direct()` factories and accessors → implement.
3. **Refactor `Entry` to carry `ConfigStrategy` + baseline `Value`** — no behaviour change; existing tests stay green.
4. **TDD: `mergeOverlay(baseline, overlay)` helper** — pure function over `Value` trees: leaf override regardless of value, recursive group merge, empty overlay no-op.
5. **TDD: `selectStrategy()` pure helper** — per-category strategy from `(category, ideDir, userDataDir, readOnly, explicitConfigPath)`.
6. **Add `userDataDirOverride` test seam** — single optional ctor param, used only by tests.
7. **Wire `mergeOverlay` into `load()`** — end-to-end overlay loading; integration tests for READONLY vs portable routing.
8. **TDD: `diffAgainstBaseline(merged, baseline)` helper** — returns subset of merged whose leaves differ; deletion semantics deferred to user-edited overlay.
9. **Rewrite `save()` around `ConfigStrategy`** — diff+prune for Overlay, direct write for Direct; preserve hand-edited overlay comments via read-before-write.
10. ~~**TDD: `migrateLegacyMirror` helper**~~ — skipped: pre-release versions only, no migration story owed to users.
11. **Delete `copyMissingResources`** + reduce `hasReadOnlySentinel` to routing flag.
12. **Update `reloadIfKnown`** to match base OR overlay path per category; preserve Config cascade.
13. **Clarify + implement `setCategoryPath` / `reloadConfig`** under layered model (former = new bundle + derived overlay; latter = Direct, matching `--config=PATH` semantics at runtime).
14. **TDD: two-dir theme enumeration + `absolute()` precedence** — user themes win on collision; portable mode = single dir.
15. **Locate theme copy-on-modify trigger** and wire duplicate-then-repoint flow; TDD the pure helpers (`isBundleTheme`, duplicate-path resolution); manual smoke for the UI.
16. **Manual smoke verification on .app bundle** — fresh launch, mutate, restart, simulate bundle update, simulate legacy migration.
17. **Update `change-log.md`** with the user-visible behaviour change.
