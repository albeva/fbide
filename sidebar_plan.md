# Sidebar "Browser" plan

Reintroduces left-dock sidebar from old fbide. First cut: file-browser tab only.
Sub/Function tree and list deferred.

## Goal

Left-dock AUI pane named "Browser" containing a `wxAuiNotebook` with one tab
("Browse Files") wrapping `wxGenericDirCtrl`. Toggleable via View menu (mirrors
Output/Result). Right-click on a document tab shows "Locate in Browser" which
reveals the current file in the tree.

## Architecture

### New module — `src/lib/sidebar/SideBarManager.{hpp,cpp}`

- Owned by `Context`, declared after `m_uiManager` so destructor runs before
  `UIManager` destroys the frame.
- `attach(wxAuiNotebook* notebook)` — two-phase init invoked from
  `UIManager::createLayout` once the notebook exists. Builds the Browse Files
  tab: `wxGenericDirCtrl` parented to the notebook (Unowned).
- `locateFile(const wxString& path)` — switches to Browse Files tab,
  `ExpandPath` + `SelectPath`. Caller responsible for showing the pane.
- `wxEVT_DIRCTRL_FILEACTIVATED` handler → `DocumentManager::openFile`.

### Context integration

- Add `std::unique_ptr<SideBarManager> m_sideBarManager`, declared right after
  `m_uiManager`.
- `getSideBarManager()` accessor.

### CommandId + CommandManager

- `CommandId::Browser` next to `Result`.
- Register `CommandEntry { id=Browser, name="viewBrowser", kind=wxITEM_CHECK }`.
- No custom event handler — `onAnyEvent` plus `CommandEntry::update()`'s
  `wxAuiManager*` visitor already shows/hides AUI panes by name.
- `CommandManager::initializeCommands` restores checked state from
  `commands.viewBrowser`.

### UIManager::createLayout

- `m_sideBar = make_unowned<wxAuiNotebook>(m_frame, wxID_ANY, ..., wxAUI_NB_TOP)`.
- `m_aui.AddPane(m_sideBar, wxAuiPaneInfo().Name(kBrowserPaneName).Caption(tr("sidebar.title")).Left().BestSize(220,-1).Hide())`.
- Push `&m_aui` into the Browser CommandEntry binds (mirrors Output line 445).
- Call `m_ctx.getSideBarManager().attach(m_sideBar)` to populate.
- AUI pane `Name()` MUST equal CommandEntry `name` string — share via
  `inline constexpr auto kBrowserPaneName = "viewBrowser"`.

### Document tab context menu

- Bind `wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN` on `UIManager::m_notebook`.
- Handler: identify document via `getPage(idx)` → `DocumentManager::findByEditor`.
  Skip if doc is new/unsaved (no path to locate). Popup `wxMenu` with one item.
- On click: `cmd.find(+CommandId::Browser)->setChecked(true)` (shows pane) +
  `m_ctx.getSideBarManager().locateFile(doc->getFilePath())`.

### Resources

- `layout.ini`: `view=settings,format,viewResult,viewBrowser,viewSubs,compilerLog`.
  Skip toolbar entry until icon decided.
- `config_win.ini`, `config_linux.ini`, `config_macos.ini`: add `viewBrowser=0`.
- `locales/en.ini` and `locales/et.ini`:
  - `[commands/viewBrowser]` `name=Browser`, `help=Toggle browser sidebar`
  - `[sidebar]` `title=Browser`
  - `[sidebar/tabs]` `browseFiles=Browse Files`
  - `[tabContext]` `locateInBrowser=Locate in Browser`
- `shortcuts_*.ini`: skip for now.
- `ArtiProvider`: skip until toolbar entry added.

### CMake

- Add `sidebar/SideBarManager.cpp` to `src/lib/CMakeLists.txt`.

## Critical review

1. Two-phase init (Context constructs empty, UIManager attaches notebook later)
   is awkward but mirrors the OutputConsole shape. Acceptable.
2. Member declaration order matters: SideBarManager must be declared after
   UIManager so it destructs first. Comment in `Context.hpp`.
3. Pane name vs CommandEntry name string — two literals must agree. Mitigate
   with shared constant.
4. Locate when pane hidden: `setChecked(true)` triggers AUI Show + Update via
   the existing visitor. Verified.
5. Don't show "Locate" for new/unsaved documents (no file path).
6. `wxGenericDirCtrl` filter: empty (show everything), matching old fbide.
7. Initial root: defaults to system roots (drives on Windows, `/` on Linux).
8. wxGTK cosmetic quirks accepted (already in todo list).
9. Toolbar entry deferred to avoid icon dependency.
10. AUI pane geometry not persisted — out of scope (Output isn't either).
11. `Subs` / `viewSubs` already exists for the future Sub/Function browser.
    Keep separate.

## TODOs

1. Add `viewBrowser=0` to `resources/ide/config_win.ini`, `config_linux.ini`,
   `config_macos.ini`.
2. Add `viewBrowser` to `view=` line in `resources/ide/layout.ini` (under
   `[menu]`).
3. Add locale strings (`commands.viewBrowser`, `sidebar.title`,
   `sidebar.tabs.browseFiles`, `tabContext.locateInBrowser`) to
   `locales/en.ini` and `locales/et.ini`.
4. Add `CommandId::Browser` to `command/CommandId.hpp`.
5. Register `CommandEntry { Browser, "viewBrowser", wxITEM_CHECK }` in
   `CommandManager` ctor.
6. Define shared constant `kBrowserPaneName = "viewBrowser"` in sidebar header.
7. Create `src/lib/sidebar/SideBarManager.{hpp,cpp}`:
   - ctor `(Context&)`, default state empty
   - `attach(wxAuiNotebook*)` builds Browse Files tab with `wxGenericDirCtrl`
   - `locateFile(const wxString&)` selects + expands path
   - `wxEVT_DIRCTRL_FILEACTIVATED` → `DocumentManager::openFile`
8. Add `m_sideBarManager` to `Context` (declared right after `m_uiManager`),
   add `getSideBarManager()`, construct in init list.
9. In `UIManager::createLayout`: create sidebar `wxAuiNotebook` (Unowned, left
   dock, hidden), add AUI pane `Name(kBrowserPaneName)`, push `&m_aui` into
   Browser CommandEntry binds, call `attach()`.
10. In `UIManager` (notebook event owner): bind
    `wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN` on `m_notebook`. Handler builds wxMenu
    with single "Locate in Browser" item; skip if doc is new. On click:
    `setChecked(true)` on Browser entry + `locateFile()`.
11. Add `sidebar/SideBarManager.cpp` to `src/lib/CMakeLists.txt`.
12. Build + smoke test: toggle, persistence across restart, locate from tab,
    double-click open from tree.
13. Update `rewrite-todo.md`: add sidebar entry.
