# `document/` cleanup — phased plan

Pre-work before project support starts. Six issues identified in the
senior-engineer review collapse into **five phases**, each
independently shippable and safe to commit-and-push between steps.

## Goal

Shrink `DocumentManager` to its actual job (own documents, drive IO
and intellisense), kill the small UX bug in the frame-title path,
remove a few SRP leaks, and clear out the leftover `const_cast` —
so subsequent project work has a clean document layer to extend.

## Why these and not others

The review surfaced ~25 items. The ones below were chosen because
they:

- Fix a real user-visible bug (frame title).
- Remove cross-layer coupling that would otherwise leak into
  whatever projects-layer code we write next (`syncEditCommands`,
  status-bar field writes).
- Reduce `DocumentManager`'s public surface (Find/Replace extraction)
  so project-support code doesn't have to grow it further.

Everything else (minimap split, `m_compiledFile` relocation,
unordered-map lookups, doc-style smoothing, …) is deferred — none
of it blocks project work and most has clear "do when surrounding
code is touched" triggers.

---

## Phase 1 — `Document::getFrameTitle()` + active-only frame-title update

**Issues:** review items 1 (frame-title bug) + 6 (duplicate
derivation).

### Scope

Two call sites today derive the frame title from a document:

```cpp
// DocumentManager::refreshTitleFor:
m_ctx.getUIManager().setTitle(doc.isNew() ? doc.getTitle() : toWxString(doc.getFilePath()));
// DocumentNotebook::onPageChanged:
m_ctx.getUIManager().setTitle(doc->isNew() ? doc->getTitle() : toWxString(doc->getFilePath()));
```

And `refreshTitleFor` writes the frame title regardless of whether
`doc` is the active tab — so `saveAllFiles()` ends with the title
pointing at the last-saved doc rather than the active one.

### Design

- Add `auto Document::getFrameTitle() const -> wxString;` returning
  exactly today's derivation (`isNew() ? getTitle() : toWxString(path)`).
- Both call sites use it.
- `refreshTitleFor` guards the title write with `if (&doc == getActive())`.

### Files

- `document/Document.hpp`, `document/Document.cpp` (new method, ~3 lines).
- `document/DocumentManager.cpp` (`refreshTitleFor` uses the helper +
  active guard).
- `document/DocumentNotebook.cpp` (`onPageChanged` uses the helper).

### Risks

Behavioural change in `saveAllFiles` is intentional — the frame
title now tracks the focused tab, not whichever doc was saved last.
Manual smoke: save-all with several dirty tabs, confirm frame title
stays on the active one.

---

## Phase 2 — `UIManager::clearDocumentStatus()`

**Issue:** review item 5.

### Scope

`DocumentManager::closeFile` writes empty strings into status-bar
fields 1-4 directly when the last doc closes. The schema (how many
fields, which are document-scoped) is UIManager's responsibility.

### Design

- Add `void UIManager::clearDocumentStatus()` that wipes the
  document-scoped fields (1-4 today; the method owns the index list).
- `DocumentManager::closeFile` calls `m_ctx.getUIManager().clearDocumentStatus()`
  instead of `frame->SetStatusText("", N)` four times.

### Files

- `ui/UIManager.hpp`, `ui/UIManager.cpp` (new method).
- `document/DocumentManager.cpp` (replace the four direct writes).

### Risks

None — pure encapsulation move.

---

## Phase 3 — Drop the `const_cast` in `onIntellisenseResult`

**Issue:** review item 3.

### Scope

```cpp
auto* doc = const_cast<Document*>(result.owner); // NOLINT(...)
doc->setSymbolTable(result.symbols);
```

`IntellisenseResult::owner` is typed `const Document*`. The worker
already stores a non-const `Document*` internally (per
`IntellisenseService.hpp:65`); only the result type tightens it
unnecessarily.

### Design

- Change `IntellisenseResult::owner` from `const Document*` →
  `Document*`. The worker still treats it as opaque (never
  dereferences); the UI-thread receiver mutates `setSymbolTable`.
- Drop the `const_cast` + NOLINT in `DocumentManager::onIntellisenseResult`.
- `DocumentManager::contains(const Document*)` still accepts const
  pointers; the result-owner check passes through unchanged.

### Files

- `analyses/intellisense/IntellisenseService.hpp` (one-word change in
  the struct).
- `document/DocumentManager.cpp` (drop the cast + NOLINT).

### Risks

None — the cast was the only suppression hiding the type
mismatch. After: types line up.

---

## Phase 4 — Move `syncEditCommands` to `CommandManager`

**Issue:** review item 1b.

### Scope

`DocumentManager::syncEditCommands` enables / disables a fixed set
of edit `CommandId`s (Undo, Redo, Cut, Copy, Paste, SelectAll) from
the active editor's state. Knowing about specific `CommandId`s
isn't DocumentManager's job — that's exactly what `CommandManager`
is for.

### Design

- New method `void CommandManager::syncEditCommands(const Editor* active);`
  - When `active == nullptr`, clears the force-disabled mask on each id
    (today's "no editor" branch).
  - Otherwise, derives the per-command mask from the editor's state
    (`CanUndo`, `CanRedo`, selection, paste availability, length).
- Update call sites:
  - `UIManager::onStatusBarClick` (2 sites): pass `dm.getActive() ? doc->getEditor() : nullptr`.
  - `DocumentNotebook::onTabRightDown`: same.
- Delete `DocumentManager::syncEditCommands` (public method + body).

### Files

- `command/CommandManager.hpp`, `command/CommandManager.cpp` (new
  method, ~30 lines moved across).
- `document/DocumentManager.hpp`, `document/DocumentManager.cpp`
  (remove method).
- `ui/UIManager.cpp` (2 call sites updated).
- `document/DocumentNotebook.cpp` (1 call site updated).
- `command/CommandEntry.hpp` doc comment (already mentions
  `DocumentManager::syncEditCommands`; update reference).

### Risks

Mechanical move. Smoke check: open a file, exercise Undo/Redo/Cut/Copy/
Paste/SelectAll enabled state across focus changes.

---

## Phase 5 — Extract `EditorSearchService`

**Issue:** review item 1a. Largest carve-out, ~100 LOC out of
`DocumentManager`. Split into three commits for easier review.

### Scope

Find / Replace / Goto Line is editor-bound functionality that
operates on the active editor and never touches the document
collection. Today it lives in `DocumentManager` only because
that's where the wxFindReplaceData and the event handlers
landed historically.

### Design

- New class `EditorSearchService` in `src/lib/editor/`:
  ```cpp
  class EditorSearchService final : public wxEvtHandler {
  public:
      NO_COPY_AND_MOVE(EditorSearchService)
      explicit EditorSearchService(Context& ctx);

      void showFind();
      void showReplace();
      void findNext();
      void gotoLine();

  private:
      void showFindDialog(bool replace);
      void onFindDialog(wxFindDialogEvent& event);
      void onFindDialogNext(wxFindDialogEvent& event);
      void onReplaceDialog(wxFindDialogEvent& event);
      void onReplaceAllDialog(wxFindDialogEvent& event);
      void onFindDialogClose(wxFindDialogEvent& event);

      Context& m_ctx;
      wxFindReplaceData m_findData { wxFR_DOWN };
      wxDECLARE_EVENT_TABLE();
  };
  ```
- Owned by `Context` as a top-level service (matches `SideBarManager`
  / `HelpManager` pattern), accessor `getEditorSearchService()`.
- `CommandManager` call sites (`onFind`, `onFindNext`, `onReplace`,
  `onGotoLine` — 4 sites) switch from
  `m_ctx.getDocumentManager().showFind()` etc. to
  `m_ctx.getEditorSearchService().showFind()`.
- `DocumentManager` drops the public methods, private helper, event
  table, `m_findData` field, and the 5 `EVT_FIND_*` macros.
- `DocumentManager` keeps `wxEvtHandler` inheritance — still needed
  for `EVT_INTELLISENSE_RESULT` Bind.

### Sub-phases

- **5a.** Scaffold `EditorSearchService` (header + empty .cpp +
  CMake). Compiles, nothing uses it.
- **5b.** Implement the class — copy logic across from
  `DocumentManager`, both sources coexist temporarily. Wire into
  `Context`.
- **5c.** Migrate `CommandManager` to the new accessor, delete the
  moved code from `DocumentManager`.

### Files

- New: `editor/EditorSearchService.{hpp,cpp}`, CMake entries.
- Modify: `app/Context.{hpp,cpp}`, `command/CommandManager.cpp`,
  `document/DocumentManager.{hpp,cpp}`.

### Risks

- The dialog uses `PushEventHandler(this)` — the new service must
  push itself, not DocumentManager. Verify in 5b.
- Find dialog state (`m_findData`) carries the last search across
  invocations; preserved by moving the field whole.
- Smoke check: Find / Find Next / Replace / Replace All / Goto Line
  across multiple tabs.

---

## Critical review of the plan

### What's good

- Each phase is small, independently shippable, and reversible.
- Phase 1 (the user-visible bug) lands first; the bigger refactor
  comes last when patterns are settled.
- Same A→G discipline as the `DocumentNotebook` extraction —
  scaffold → wire → migrate within phase 5.
- Test surface is honest: most phases can only be smoke-tested
  (wx GUI plumbing); the plan calls that out instead of pretending
  to add unit tests for thin wrappers.

### Trade-offs I'm deliberately accepting

- Phase 1 includes a small behaviour change (`saveAllFiles` no
  longer flips frame title to the last-saved doc). Documented as
  intentional in the commit message; nothing else depends on the
  old behaviour.
- Phase 4's API choice (caller passes `Editor*`) trades a tiny bit
  of caller verbosity for keeping `CommandManager` unaware of
  `DocumentManager`. Worth it.
- Phase 5 puts `EditorSearchService` under `Context` rather than
  inside `DocumentManager` as a member. Costs one more accessor;
  buys a cleaner mental model (search is a top-level service) and
  removes the "DocumentManager forwards to internal helper" noise.

### Risks I'm NOT mitigating up front

- **wxFindReplaceDialog lifecycle**: today's code creates the
  dialog with `make_unowned`, pushes `this` as an event handler,
  and relies on `onFindDialogClose` to pop + destroy. Moving the
  handler to a different `wxEvtHandler` is mechanical but easy
  to get wrong; verify by exercising the close path.
- **Phase 4 timing**: `syncEditCommands` is called from
  `DocumentNotebook::onTabRightDown`. That event fires before any
  CommandManager-level wiring exists for the new method, so the
  order in Phase 4 matters: add `CommandManager::syncEditCommands`
  first, switch the call sites, *then* delete the DocumentManager
  method. Same pattern as the notebook extraction's step C → D → E.

### What I considered and rejected

- **Folding all changes into one PR**: rejected. Phase 1 is a
  user-visible bug fix that deserves to land on its own. Phase 5
  is large enough that bisecting a regression to a single commit
  is worth the extra cost.
- **Doing Phase 5 first** (clean DocumentManager once, then small
  fixes on top): rejected because Phase 1's bug is real and should
  ship sooner. The small fixes don't fight Phase 5; they touch
  different methods.
- **Renaming `refreshTitleFor`** during Phase 1: rejected as
  scope-creep. It's a clear enough name; rename only if a follow-up
  finds it ambiguous.
- **Pulling `findByPath` / `findByEditor` into typed lookup maps**
  during this cleanup (review item 12): rejected — pre-optimization
  for small N, no current pain.

---

## TODO sequence

Each phase = one self-contained commit + push, with clang-tidy on
touched code and tests + smoke before commit. Phase 5 spans three
commits internally.

1. **Phase 1** — `Document::getFrameTitle()` + active-only frame-title fix.
2. **Phase 2** — `UIManager::clearDocumentStatus()` + drop direct field writes.
3. **Phase 3** — Drop `const_cast` in `onIntellisenseResult` (flip `IntellisenseResult::owner` to non-const).
4. **Phase 4** — Move `syncEditCommands` to `CommandManager(Editor*)`; migrate 3 call sites.
5. **Phase 5a** — Scaffold `EditorSearchService` (empty class + CMake).
6. **Phase 5b** — Implement the service; wire into `Context`. Both sources coexist.
7. **Phase 5c** — Migrate `CommandManager` call sites; delete the moved code from `DocumentManager`.

After phase 7: `DocumentManager` is down to its core job, no
`const_cast`, no UI-field reach-through, no `CommandId` knowledge,
no Find/Replace state. Ready to extend with project-aware behaviours.
