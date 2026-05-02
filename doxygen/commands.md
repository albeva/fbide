# Commands {#commands}

A *command* is the unit of user-visible action: New File, Compile,
Toggle Browser, etc. Each command has one `CommandId` enum value, one
internal name (e.g. `"compile"`), and zero or more *bindings* — the
concrete UI controls (menu items, toolbar tools, AUI panes, config
toggles) that surface it.

## The model

`fbide::CommandEntry` (`src/lib/command/CommandEntry.hpp`) holds:

| Field            | What                                                    |
|------------------|---------------------------------------------------------|
| `id`             | `wxWindowID` — usually a `CommandId` enum value.        |
| `name`           | String key used by layout, locale, and shortcuts.       |
| `kind`           | `wxItemKind` — Normal, Check, Dropdown.                 |
| `enabled`        | Broad UI gate (set by `UIManager::applyState`).         |
| `forceDisabled`  | Per-editor mask (set by `DocumentManager`).             |
| `checked`        | Toggle state for `wxITEM_CHECK` entries.                |
| `binds`          | `vector<variant<wxMenu*, wxMenuItem*, wxToolBarToolBase*, wxAuiManager*, ConfigManager*>>` |

`CommandManager` (`src/lib/command/CommandManager.hpp`) owns every entry
in two lookup tables: by name, and by id. The constructor seeds the
table with every command the IDE supports; layout/UI code then attaches
binds as it builds the menu and toolbar.

## Resolution chain

A single command's behaviour is assembled from five files plus two C++
sites:

| Source                  | Section / key            | Contributes              |
|-------------------------|--------------------------|--------------------------|
| `layout.ini`            | `toolbar=`, `[menu]/...` | Existence + position     |
| `locales/<lang>.ini`    | `[commands/<name>]`      | Display name / tooltip   |
| `shortcuts_<plat>.ini`  | `<name>=Ctrl+...`        | Accelerator              |
| `config_<plat>.ini`     | `[commands]/<name>`      | Initial check state      |
| `CommandId.hpp`         | enum value               | wx event id              |
| `CommandManager.cpp`    | `wxBEGIN_EVENT_TABLE`    | Static dispatch handler  |

The five INI files merge into a single logical config tree via
`ConfigManager::Category` — see @ref settings.

## Worked example: `compile`

```
layout.ini      : toolbar = ...,compile,... ; [menu] run = compile,...
locales/en.ini  : [commands/compile] name="&Compile" help="..." accel="..."
shortcuts.ini   : compile = F5
CommandId.hpp   : enum CommandId { ..., Compile, ... }
CommandManager  : addCommands({ {.id=+CommandId::Compile, .name="compile"} })
                  EVT_MENU(+CommandId::Compile, CommandManager::onCompile)
UIManager       : while building "run" menu, lookup "compile" by name,
                  attach the wxMenuItem* / wxToolBarToolBase* to entry.binds
runtime         : F5 / click → wxEVT_MENU id=+CommandId::Compile
                  → CommandManager::onCompile
                  → m_ctx.getCompilerManager().compile()
```

The entry stays alive in `m_namedCommands`; binds accumulate as the
control is created. From then on, `setEnabled` / `setChecked` /
`setForceDisabled` propagate state across every bound control via
`CommandEntry::update()`.

## State model

Two independent gates govern whether a command can fire:

- **`enabled`** — broad context. Set by `UIManager::applyState(UIState)`
  in response to compile/run lifecycle changes. Example: `Compile` and
  `Run` flip off while a build is in flight, on again when it ends.
- **`forceDisabled`** — fine-grained per-editor mask. Set by
  `DocumentManager::syncEditCommands` from the active editor's state:
  `CanUndo`, `CanPaste`, has-selection, etc. Example: `Paste` is
  forceDisabled while the clipboard has nothing useful.

Effective state is `enabled && !forceDisabled` (see
`CommandEntry::isEnabled()`). Keep the two layers separate when adding
new state-driven commands — don't reach into `enabled` from per-editor
sync code, and don't reach into `forceDisabled` from the UI gate.

## Dispatch

`CommandManager` is itself a `wxEvtHandler` and lives long enough to
serve every `wxEVT_MENU` for the application. The static event table
maps each `CommandId` to a member handler. There are two range
dispatches for dynamic ID slots:

- `EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, onFileHistory)` — the
  recent-files submenu, populated by `wxFileHistory`.
- `EVT_MENU_RANGE(ExternalLinkFirst, ExternalLinkLast,
  onExternalLink)` — externally configured Help-menu links.
  `registerExternalLink(url)` reserves the next free ID; that mapping
  lives in `m_externalLinks` and survives until `clearExternalLinks()`.

`onAnyEvent` runs first (before dispatch) and is what keeps
`wxITEM_CHECK` entries' internal `checked` flag in sync with the toolbar
toggle the user just clicked. `onAuiPaneClose` does the same job for
AUI-pane toggles when the user closes a pane via its `X` button.

## Recipe: add a new command

1. Add a value to `enum class CommandId` in `CommandId.hpp`.
2. Add an `EVT_MENU(+CommandId::Foo, onFoo)` row to
   `CommandManager.cpp`'s event table; declare and implement `onFoo`.
3. Add a `CommandEntry { .id=+CommandId::Foo, .name="foo" }` to the
   `addCommands({ ... })` block in `CommandManager`'s constructor.
4. Wire layout: add `foo` to the relevant menu / toolbar entry in
   `layout.ini`.
5. Add display strings under `[commands/foo]` in every
   `locales/<lang>.ini`.
6. Add an accelerator under `shortcuts_<plat>.ini` if applicable.
7. (Check kind only) seed the default in `config_<plat>.ini` under
   `[commands]/foo`.

The handler implementation is just dispatch — keep logic out of
`CommandManager` and call into the appropriate manager via `m_ctx`.

## Cross-links

- @ref ui — how `UIManager` builds menu/toolbar from `layout.ini` and
  attaches bindings.
- @ref settings — `ConfigManager` category model that backs the five
  config files.
- @ref documents — `syncEditCommands` and the `forceDisabled` mask.
- @ref compiler — `UIState` flips that drive `applyState` for
  Compile/Run.
