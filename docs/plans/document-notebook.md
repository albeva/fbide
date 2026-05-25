# DocumentNotebook refactor — plan

Branch: `project-support`. Pre-work before adding proper project support.

## Goal

Lift the document tab strip out of `UIManager` into a dedicated
`DocumentNotebook` class, so each layer's responsibility is clean:

- **`UIManager`** — frame, menus, toolbar, status bar, AUI dock.
  Knows *nothing* about the document notebook beyond docking it.
- **`Document`** — one open file: editor widget + metadata.
- **`DocumentManager`** — owns the document collection + IO + intellisense.
- **`DocumentNotebook`** — tab UI: open / close / focus / context menu / titles.

## Design

```cpp
class DocumentNotebook final : public wxAuiNotebook {
public:
    NO_COPY_AND_MOVE(DocumentNotebook)
    DocumentNotebook(wxWindow* parent, Context& ctx);

    void addPage(Document& doc, bool select = true);
    void removePage(Document& doc);
    void selectDocument(Document& doc);
    void updateTitle(const Document& doc);

    [[nodiscard]] auto activeDocument() const -> Document*;
    [[nodiscard]] auto documentForPage(const wxWindow* page) const -> Document*;

private:
    void onPageClose(wxAuiNotebookEvent& event);
    void onPageChanged(wxAuiNotebookEvent& event);
    void onBgDClick(wxAuiNotebookEvent& event);
    void onTabRightDown(wxAuiNotebookEvent& event);

    Context& m_ctx;
    wxDECLARE_EVENT_TABLE();
};
```

### Ownership

- Widget lifetime: wx parent (the frame). Standard wx pattern, same as
  `SideBarManager`'s sidebar notebook.
- `DocumentManager` holds `Unowned<DocumentNotebook> m_notebook` — a
  non-owning pointer; "manages" rather than "owns" in the destructor
  sense.
- `UIManager` holds no pointer to it at all.

### Construction

Two-phase init, matching today's `attachNotebook` pattern. The frame
doesn't exist when `DocumentManager` is constructed, so a separate
initializer creates the widget once `UIManager::createMainFrame` runs:

```cpp
auto DocumentManager::createNotebook(wxWindow* parent) -> DocumentNotebook&;
[[nodiscard]] auto DocumentManager::notebook() -> DocumentNotebook&;
```

`UIManager::createLayout` calls `dm.createNotebook(m_frame)`, docks the
returned pointer into the AUI centre pane, and never references it
again.

## Responsibility map

| Concern | Before | After |
|---|---|---|
| Construct `wxAuiNotebook` | `UIManager::createLayout` | `DocumentNotebook` ctor |
| Add/remove pages | `DocumentManager` → `getNotebook()->AddPage/DeletePage` | `DocumentNotebook::addPage / removePage` |
| Active document lookup | `DocumentManager::getActive` walks notebook | `DocumentNotebook::activeDocument`; `DocumentManager::getActive` forwards |
| Title updates | `DocumentManager::updateTabTitle` reaches in | `DocumentNotebook::updateTitle` |
| `findByPage` / `findPageIndex` | `DocumentManager` | `DocumentNotebook` (private + `documentForPage`) |
| `PAGE_CLOSE` / `PAGE_CHANGED` / `BG_DCLICK` | `UIManager` event table | `DocumentNotebook` event table |
| `TAB_RIGHT_DOWN` + context menu | `DocumentManager::attachNotebook` + `onTabRightDown` | `DocumentNotebook` |
| `UIManager::getNotebook()` / `m_notebook` / `DocumentTabsId` | exists | deleted |
| `DocumentManager::getNotebook()` private helper / `attachNotebook()` | exists | deleted |
| `FileSession` active-tab read / write | via `UIManager::getNotebook()` | via `dm.notebook()` |
| `refreshAuiArt` art-provider refresh | reads `m_notebook->GetArtProvider()` | reads `dm.notebook().GetArtProvider()` |

`findByEditor` stays on `DocumentManager` — `Editor` uses it for
non-notebook purposes.

## Migration steps

Each step builds cleanly on its own; OK to commit + verify between
steps or ship as one PR.

- **A. Scaffold `DocumentNotebook`.** New `src/lib/document/DocumentNotebook.{hpp,cpp}`. Inherits `wxAuiNotebook`; ctor takes `(wxWindow*, Context&)`; empty body. CMake additions. Compiles; nothing references it.
- **B. Wire into `DocumentManager`.** Add `Unowned<DocumentNotebook> m_notebook` (declared *before* `m_intellisense` so intellisense still tears down first). Add public `createNotebook(parent)` + `notebook()`. Old private `getNotebook()` helper stays during transition.
- **C. Implement `DocumentNotebook` surface + event handlers.** Public methods (`addPage` / `removePage` / `selectDocument` / `updateTitle` / `activeDocument` / `documentForPage` / `activeIndex` / `pageCount`). Event table for `PAGE_CLOSE` / `PAGE_CHANGED` / `BG_DCLICK` / `TAB_RIGHT_DOWN` + the right-click menu (translated from `DocumentManager::onTabRightDown`). Handlers route into `DocumentManager` via `Context`.
- **D. Move notebook out of `UIManager`.** `createLayout` calls `dm.createNotebook(m_frame)` and docks the result. Drop `UIManager`'s `m_notebook`, `getNotebook()`, `DocumentTabsId`, and the three notebook event handlers. Update `refreshAuiArt`.
- **E. Migrate `DocumentManager` internals.** Replace every notebook touch with calls through `m_notebook`. Drop `getNotebook()` private helper and `attachNotebook()`. Move `findByPage` / `findPageIndex` / `updateTabTitle` into `DocumentNotebook` (`findByPage` drops from public API; no external consumers remain). `Document` constructed with `DocumentNotebook*` as wx parent.
- **F. Route `FileSession`.** Replace the three `UIManager::getNotebook()` reads/writes with `dm.notebook().GetSelection() / SetSelection` (or the typed `activeIndex()` / `selectDocument`).
- **G. Build + test + smoke.** Unit tests, UI tests, app launch — open / close / switch tabs, double-click empty strip for new file, save+reload session.

## Risks

- **Construction order**: any call to `dm.notebook()` *before* `createNotebook(parent)` runs will crash. Same constraint as today's `getNotebook()`; document on the accessor + assert in debug.
- **`Document` parent change**: today the page is parented to `wxAuiNotebook*`; after refactor it's parented to `DocumentNotebook*` (which IS a `wxAuiNotebook`). Verify the wx tree behaves the same in practice.
- **Context-menu action lambdas** capture `this` (DocumentManager) today; after the move they capture `&m_ctx.getDocumentManager()`. One more level of indirection, no semantic change.
