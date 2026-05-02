# Settings {#settings}

Settings are split into two layers: the **dialog** the user opens
(`Edit → Preferences`) and the **store** that backs every config
read in the codebase. The dialog edits the store; the store
publishes changes back through `ConfigManager`'s reload path.

## SettingsDialog

`fbide::SettingsDialog` (`src/lib/settings/SettingsDialog.hpp`) is a
notebook of four panels:

| Page enum value | Panel class      | Backs                                                |
|-----------------|------------------|------------------------------------------------------|
| `General`       | `GeneralPage`    | UI font, splash, EOL/encoding defaults, transforms.  |
| `Theme`         | `ThemePage`      | Theme picker + per-category color/style editing.     |
| `Keywords`      | `KeywordsPage`   | Keyword group editor + case rules.                   |
| `Compiler`      | `CompilerPage`   | `fbc` path probe, compile/run templates, params.     |

`create(initial)` constructs the panels and selects the requested
page. Callers who want to land on a specific tab use the
`Page` enum:

```cpp
SettingsDialog dlg(parent, ctx);
dlg.create(SettingsDialog::Page::Compiler);
dlg.ShowModal();
```

`CompilerManager`'s "fbc not found" prompt uses this to drop the user
directly on the Compiler tab.

## Apply / save chain

Each page has its own `apply()` that writes back into
`ConfigManager`'s `Value` tree — no per-control persistence layer,
no commit/rollback complexity. The dialog only commits on **OK**:

```
ShowModal() == wxID_OK
    │
    ▼
applyChanges()
    │
    ├─ generalPage.apply()
    ├─ themePage.apply()
    ├─ keywordsPage.apply()
    └─ compilerPage.apply()
        │
        ▼   each page mutates the Value tree it cares about
ConfigManager::save(Category::*)   ← per category, only the dirty ones
    │
    ▼
write each category file to disk
    │
    ▼
refreshUi() / updateEditorSettings()  ← propagate to live UI
```

Cancel discards everything — the panels never touched anything other
than their own UI controls until `apply()` ran.

## ConfigManager categories

`ConfigManager` (`src/lib/config/ConfigManager.hpp`) is a multi-file
INI store. Five categories share a single `Value` tree shape but
live in separate files:

| Category    | Backing file (default)     | Content                           |
|-------------|----------------------------|-----------------------------------|
| `Config`    | `config_<plat>.ini`        | All editor / compiler settings.   |
| `Locale`    | `locales/<lang>.ini`       | Display strings (i18n).           |
| `Shortcuts` | `shortcuts_<plat>.ini`     | Keyboard accelerators.            |
| `Keywords`  | `keywords.ini`             | FreeBASIC keyword groups.         |
| `Layout`    | `layout.ini`               | Menu / toolbar wiring.            |

`ConfigManager::get(Category)` returns the root `Value` for that
category; the typed accessors `config()`, `locale()`, `shortcuts()`,
`keywords()`, `layout()` are sugar over the same dispatch.

`Theme` is owned directly (`m_theme`), not as part of the `Value`
tree — it has its own typed schema (see @ref theming).

## Hot reload chain

`reloadIfKnown(path)` is the public hook for "the user just saved a
file we care about". `DocumentManager::saveFile` calls it after every
successful save. If `path` matches one of:

- An active category file (`config_*.ini`, `keywords.ini`,
  `locales/<lang>.ini`, `shortcuts_*.ini`, `layout.ini`),
- The active theme file,

then the relevant data is reloaded from disk and the cascade fires:

```
reloadIfKnown(path)
    │
    ├─ load(Category::*) for the matched category
    │   OR Theme::load() for the matched theme
    │
    ▼
UIManager::refreshUi()         (rebuild menu/toolbar/status bar from new locale + layout)
DocumentManager::updateEditorSettings()  (re-apply theme/keywords to every Editor)
```

This is the same code path SettingsDialog runs after OK — the dialog
just edits in-memory first, saves, then triggers the reload.

## `--cfg=<spec>` introspection

`App::resolveCfg` reads from the same store. The CLI side is documented
in `App::OnInit`; the takeaway here is that any value the dialog can
edit is also reachable from `fbide --cfg=<spec>`, including
enumeration markers (`*`, trailing `/`).

## Recipe: add a new settings field

1. Add the key to `config_<plat>.ini` with a sensible default.
2. Pick the panel that owns the field — extend its UI with a control.
3. Add the model member to the panel (loaded from `ConfigManager`
   in the panel's constructor).
4. Implement read in the panel's `apply()` — write back via
   `m_ctx.getConfigManager().config()[<key>] = ...`.
5. If the change should affect live editors, extend
   `DocumentManager::updateEditorSettings` (or add a new hook on the
   relevant manager).
6. If the change should affect new docs only, no further wiring is
   needed — the next read picks up the new value.
7. Document the key in `[locale] commands.<name>` if it surfaces in a
   menu.

## Cross-links

- @ref commands — config-bound `wxITEM_CHECK` commands flow through
  `ConfigManager` as one of their binds.
- @ref theming — Theme tab mechanics + `Theme` schema.
- @ref documents — encoding/EOL defaults consumed by
  `DocumentManager::defaultEncoding` / `defaultEolMode`.
- @ref compiler — Compiler tab integrates with `resolveCompilerBinary`
  + `getFbcVersion`.
