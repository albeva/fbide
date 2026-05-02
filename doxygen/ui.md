# UI {#ui}

`UIManager` builds the main application frame and owns every shared
chrome surface around it: menu bar, toolbar, status bar, AUI panes,
the document notebook, the sidebar notebook, and the output console.
`SideBarManager` populates the sidebar notebook with the
Browser / Sub-Function tabs.

## UIManager

`fbide::UIManager` (`src/lib/ui/UIManager.hpp`) is constructed by
`Context` and held as `unique_ptr<UIManager>`. The single entry point
is `createMainFrame()`, called once from `App::OnInit` after Context
construction.

### What it owns

| Member               | Wrapper             | Notes                                       |
|----------------------|---------------------|---------------------------------------------|
| `m_frame`            | `Unowned<wxFrame>`  | Top-level frame; wx parent of all chrome.   |
| `m_toolbar`          | `Unowned<wxToolBar>`| Built from `layout.ini`'s `toolbar=` line.  |
| `m_notebook`         | `Unowned<wxAuiNotebook>` | Document tabs.                         |
| `m_sideBar`          | `Unowned<wxAuiNotebook>` | Browser / Subs sidebar notebook.       |
| `m_console`          | `Unowned<OutputConsole>` | Build / error output.                  |
| `m_aui`              | `wxAuiManager` (value) | AUI dock manager for the frame.          |
| `m_artProvider`      | `unique_ptr<ArtiProvider>` | Icon dispatch.                       |
| `m_compilerLog`      | `CompilerLog*`      | Lazily created on first show.               |

`Unowned<T>` (see CLAUDE.md) is used everywhere wx-parent ownership
applies — the frame is the wx root and destroys its children.

### createMainFrame pipeline

```
createMainFrame()
    │
    ├─ instantiate frame
    ├─ createStatusBar()
    ├─ createLayout()       ← AUI panes + notebooks
    │       │
    │       └─ SideBarManager::attach(m_sideBar)
    │
    ├─ configureMenuBar()   ← walks layout.ini [menu] sections,
    │                          calls CommandManager::find(name)
    │                          to attach wxMenuItem* binds.
    ├─ configureToolBar()   ← same model, wxToolBarToolBase* binds.
    ├─ generateExternalLinks() ← Help-menu external-links submenu
    │                            via CommandManager::registerExternalLink.
    ├─ DocumentManager::attachNotebook()  ← bind tab-strip events
    └─ frame->Show()
```

`refreshUi()` re-runs the menu/toolbar configuration in place, which
is what `ConfigManager::reloadIfKnown` cascades into when the user
edits a locale or layout file. No frame teardown — labels and
shortcuts refresh on the existing controls.

## freeze() pattern

`FreezeLock` is an RAII guard around `wxWindow::Freeze` /
`wxWindow::Thaw`:

```cpp
{
    const auto thaw = m_ctx.getUIManager().freeze();  // wxFrame::Freeze
    // bulk update — repaints suppressed
}                                                       // thaw on scope exit
```

Used in `CommandManager::onAnyEvent` (every command dispatch) and
anywhere that triggers a series of layout changes. The lock cannot be
copied or moved — destruction order matches construction.

## SideBarManager

`fbide::SideBarManager` (`src/lib/sidebar/SideBarManager.hpp`) is a
two-phase initialiser:

1. Constructor runs early (during `Context::Context`) — no UI created
   yet.
2. `attach(notebook)` is called from `UIManager::createLayout` once
   the sidebar notebook exists. Builds the Browse Files
   (`wxGenericDirCtrl`) and Sub/Function (`SymbolBrowser`) tabs.

Public surface:

| Method                          | When                                                   |
|---------------------------------|--------------------------------------------------------|
| `locateFile(path)`              | Reveal a path in the directory tree.                   |
| `showSymbolsFor(doc)`           | Render `doc`'s `SymbolTable` in the Subs tab.          |
| `showSymbolBrowser()`           | Reveal the pane (toggling `viewBrowser` on if hidden) and switch to the Subs tab. Bound to `Show Subs` (F2). |

The pane name `kBrowserPaneName = "viewBrowser"` matches the
`CommandEntry` name for `CommandId::Browser`, so toggling the command
shows / hides the pane via the AUI dock.

### Lifetime gotcha

`Context` declares `m_sideBarManager` *after* `m_uiManager` so its
destructor runs first. `SideBarManager` caches a non-owning pointer
into the `wxAuiNotebook` that `UIManager`'s frame destroys; if the
order were swapped the pointer would dangle. See @ref architecture.

## applyState(UIState)

`UIState` is the broad UI-mode enum: `None`, `FocusedUnknownFile`,
`FocusedValidSourceFile`, `Compiling`, `Running`. UIManager carries
two slots:

| Slot                  | Source                              | Reset on            |
|-----------------------|-------------------------------------|---------------------|
| `m_documentState`     | `DocumentManager` (tab focus, type) | New tab focus.      |
| `m_compilerState`     | `CompilerManager` (build lifecycle) | Build done / killed.|

The effective state is `compilerState != None ? compilerState :
documentState`. `applyState(state)` walks `mutableIds[]` and toggles
each command's broad `enabled` flag — that's the wide gate.
`DocumentManager::syncEditCommands` applies the fine-grained
`forceDisabled` mask on top (CanUndo, has selection, clipboard
populated, etc.). See @ref commands for the two-layer model.

## Status bar fields

The status bar is a multi-field control owned by `UIManager`. Field
ownership is split:

| Field                       | Writer                         | Trigger                    |
|-----------------------------|--------------------------------|----------------------------|
| Status text (left)          | `CompilerManager::setStatus`   | Build state changes.       |
| Line:Col                    | `Editor::updateStatusBar`      | `EVT_STC_UPDATEUI`.         |
| EOL mode                    | `Editor::updateStatusBar`      | `EVT_STC_UPDATEUI`.         |
| Encoding                    | `Editor::updateStatusBar`      | `EVT_STC_UPDATEUI`.         |

`onStatusBarClick` opens the EOL/encoding pickers when the user clicks
those fields. UIManager creates and destroys the bar; the writers
reach in by index.

## Cross-links

- @ref architecture — Context / SideBarManager destruction order.
- @ref commands — how `applyState` and the bind list interact;
  `CommandManager` finds binds by name.
- @ref documents — `attachNotebook` and per-doc UIState mapping.
- @ref compiler — `setCompilerState` / Output Console / status text.
- @ref analyses — `SideBarManager::showSymbolsFor` is the symbol
  browser's public entry.
