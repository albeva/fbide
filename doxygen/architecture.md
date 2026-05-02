# Architecture {#architecture}

FBIde is built around a single service-locator object — `Context` — that
owns every long-lived manager. There is no DI container. The locator
pattern works here because the lifetimes are simple (one application,
one set of managers, app-scoped) and because we own the whole codebase,
so the "loose coupling" gain that DI buys you isn't worth the extra
machinery.

## The model

`fbide::Context` (`src/lib/app/Context.hpp`) owns each manager via
`std::unique_ptr` and exposes them through `getXManager()` accessors.
`App::OnInit` constructs `Context` once on startup and passes it by
reference into the managers themselves, so any manager can reach any
other through `Context&`.

```
                         App (wxApp)
                          │ owns
                          ▼
                       Context
   ┌─────────┬──────────┬────────┬──────────┬──────────┬─────────┐
   ▼         ▼          ▼        ▼          ▼          ▼         ▼
ConfigMgr  FileHist  UIManager  SideBar  DocumentMgr  Compiler  ...
                                  │
                                  └─ non-owning pointer into wxAuiNotebook
                                     (owned by UIManager's main frame)
```

Solid arrows are `unique_ptr` ownership. The dashed line is the only
non-trivial cross-reference inside `Context` itself (see §lifecycle).

## Construction order

`Context::Context` constructs members in this fixed order — the order
the dependency chain demands:

| # | Member             | Why this slot                                       |
|---|--------------------|-----------------------------------------------------|
| 1 | `ConfigManager`    | Resolves resource paths; everything else needs it.  |
| 2 | `FileHistory`      | Standalone state container.                         |
| 3 | `UIManager`        | Builds the frame; needs config to know layout.      |
| 4 | `SideBarManager`   | Caches a non-owning pointer into the frame's AUI.   |
| 5 | `DocumentManager`  | Needs UIManager's notebook.                         |
| 6 | `FileSession`      | Reads/writes session state through DocumentManager. |
| 7 | `CompilerManager`  | Reports through UIManager + watches active doc.     |
| 8 | `HelpManager`      | Independent; lazily loads help files.               |
| 9 | `CommandManager`   | Last — handlers can call into any manager above.    |

`CommandManager` lands at the end deliberately. Its job is wiring
events to handlers, and those handlers reach into every other manager.
Constructing it last guarantees the targets exist by the time wiring
starts.

## Destruction order

C++ destroys members in reverse declaration order. That's important for
`SideBarManager`: it holds a non-owning pointer into a `wxAuiNotebook`
owned by the frame that `UIManager` will destroy. By placing
`SideBarManager` *after* `UIManager` in the field list (Context.hpp),
we get `~SideBarManager` first, then `~UIManager`, and the dangling
pointer never has a chance to fire.

## Threading map

The IDE is a UI-thread application with one exception:

| Manager / service              | Thread                              |
|--------------------------------|-------------------------------------|
| `App`, `Context`, all managers | UI thread only.                     |
| `IntellisenseService`          | Worker thread + UI publish.         |

`IntellisenseService` is the only piece that crosses threads. It runs
the lex+parse+symbolize pipeline on a worker and publishes results back
to the UI thread via `wxQueueEvent`. See @ref analyses for the full
pipeline + cancellation rules. Everything else assumes "I'm on the UI
thread" and you should preserve that invariant when adding new code.

## App lifecycle

`App::OnInit` (`src/lib/app/App.cpp`) is the entry point:

1. Parse CLI (`parseCli`) — pure, fills a `CliOptions` struct.
2. Branch on `--help` / `--version` — print and exit.
3. Configure logging (debug: floating `wxLogWindow`; release: file).
4. Construct `Context(fbidePath, cli.idePath, cli.configPath)`.
5. `--cfg=<spec>` — print value, exit. No window, no IPC, no splash.
6. `InstanceHandler` check unless `--new-window` — forward files and
   exit if another FBIde is already running. (`InstanceHandler`
   itself is conditional state on `App`, not on `Context`.)
7. Splash, appearance, load `FileHistory`.
8. `UIManager::createMainFrame()` — frame, menus, toolbar, AUI panes.
9. Open positional files via `DocumentManager::openFile`.
10. `CompilerManager::checkCompilerOnStartup()` — probe `fbc`.

`App::OnExit` flushes the clipboard so text copied from FBIde survives
after the process exits.

## Cross-links

- @ref commands — how `CommandManager` wires events to handlers.
- @ref documents — `DocumentManager`'s side of the construction chain.
- @ref analyses — the only worker-thread subsystem.
- @ref ui — `UIManager` + `SideBarManager` lifetime details.
