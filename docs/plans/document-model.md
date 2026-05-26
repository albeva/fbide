# `Document` model/view split — phased plan

Pre-work for project support. Today's `Document` mixes the data model
(path, type, encoding, EOL, mod-time, symbols) with view ownership
(container panel, Editor, minimap). The mix means:

- You can't construct a `Document` without a wx parent — blocks tests
  and blocks "project tree holds documents without open tabs".
- `Document::isModified()` reaches into `m_editor->GetModify()` — fails
  when no editor exists.
- View-management code (`createMinimap`, `updateMinimapVisibility`,
  …) lives on `Document` for no better reason than "no other home
  existed".

## Target shape

```
Document            ← pure data model (no wx in ctor)
  └ m_panel        ← Unowned<EditorPanel>, nullable back-link

EditorPanel : wxPanel  ← view wrapper; owns container + widgets
  ├ Editor             (the wxStyledTextCtrl subclass)
  └ Minimap            (wxStyledTextCtrlMiniMap)

DocumentManager    ← owns Documents; creates EditorPanels on demand
```

### Key decisions

- **`EditorPanel`** as the concrete view wrapper. Skips an abstract
  `DocumentView` base for now (YAGNI) — when a second view type
  (image viewer, etc.) arrives we introduce the base then with the
  benefit of two concrete cases to shape it.
- **Text buffer lives in the view (Editor's wxSTC)**, not on Document.
  Closing a tab discards the in-memory buffer; reopening reloads from
  disk. Matches today's behaviour and avoids dual-buffer sync. A
  view-less Document is effectively "the on-disk file plus its
  metadata".
- **Back-link slot:** `Unowned<EditorPanel> m_panel` on Document.
  Typed and concrete. When a second view kind arrives we either add a
  parallel slot, introduce a small abstract base, or type-erase to
  `Unowned<wxWindow>` — pick based on what the second case actually
  needs.
- **Modified state:** `Document::isModified()` returns
  `m_metaModified || (m_panel && m_panel->isModified())`. A view-less
  Document is dirty only if its encoding/EOL were changed without a
  save round-trip.

### What stays on `Document`

`m_filePath`, `m_type` + `m_typeOverridden`, `m_encoding`, `m_eolMode`,
`m_modTime`, `m_metaModified`, `m_symbolTable`, `m_compiledFile`, the
title formatters (`getTitle`, `getFrameTitle`).

### What moves to `EditorPanel`

`m_container`, `m_editor`, `m_minimap`, `m_minimapWidth`,
`m_minimapEnabled`, `createMinimap`, `destroyMinimap`,
`updateMinimapVisibility`, `onContainerSize`, `showMinimap`,
`updateSettings`.

`Document::getEditor()` becomes a forwarder
(`return m_panel ? m_panel->getEditor() : nullptr;`) during the
transition.

---

## Phase 1 — Introduce `EditorPanel`, pure encapsulation

### Scope

Create `editor/EditorPanel.{hpp,cpp}` (inherits `wxPanel`). Move
every view-related field + method off `Document` into it. Document
still constructs `EditorPanel` in its constructor — no lifetime
change. Same public surface on Document (`getEditor`, `getPage`,
`showMinimap`, `updateSettings`) but the bodies forward.

### Files

- New: `editor/EditorPanel.{hpp,cpp}` + CMake entries.
- Modify: `document/Document.{hpp,cpp}` (~50 LOC removed → ~10 LOC of
  forwarders).

### Risks

Low. Same construction order, same wx-parenting (panel is wx-parented
to whatever Document was parented to — i.e. the `DocumentNotebook`).
Smoke check: open files, switch tabs, toggle minimap, close.

### TDD

Not yet — Document still requires a wx parent in ctor. Phase 2 makes
unit tests possible.

---

## Phase 2 — `Document` constructible without a view

### Scope

- `Document` ctor drops the `wxWindow* parent` parameter; takes just
  `(Context&, DocumentType)`.
- New public API:
  ```cpp
  void Document::attachView(Unowned<EditorPanel> panel);
  void Document::detachView();
  [[nodiscard]] auto Document::hasView() const -> bool;
  ```
- `DocumentManager::newFile` / `openFile` change construction order:
  1. Construct `Document`
  2. Construct `EditorPanel(notebook, doc, …)` with Document as a back-ref
  3. Call `doc.attachView(panel)`
  4. Add panel to notebook
- `EditorPanel` destructor calls `m_doc.detachView()` so wx parent
  destruction (e.g. notebook page close) clears the back-link cleanly.
- `Document::isModified()` / `getEditor()` / `getPage()` handle the
  `m_panel == nullptr` case — return `m_metaModified`, `nullptr`,
  `nullptr` respectively.

### Files

- Modify: `document/Document.{hpp,cpp}`,
  `document/DocumentManager.cpp` (newFile + openFile),
  `document/FileSession.cpp` (legacy openFile call path),
  `editor/EditorPanel.{hpp,cpp}` (back-ref ctor + detach in dtor).

### Risks

Medium. Every Document construction site changes. The detach-on-dtor
pattern needs care — EditorPanel must not access the Document during
the detach call (Document is a back-ref, fine to read).

### TDD

**This is the phase that unlocks unit tests for Document.** Add
`tests/unit/DocumentTests.cpp` with:

- Construct view-less Document → `hasView() == false`,
  `getEditor() == nullptr`, `getPage() == nullptr`.
- New document → `isModified() == false`, title is "Untitled".
- `setEncoding(...)` flips a flag — `isModified() == true`.
- `setModified(false)` clears the flag.
- `setFilePath(path)` updates path, re-derives type when not
  overridden.
- `setType(...)` flips override flag.

Requires a `Context` instance, which needs ConfigManager. May need a
small test fixture that builds a minimal Context — worth doing once
to enable model-side testing of every doc-layer change going forward.

---

## Phase 3 — Decouple `Document` callbacks into `DocumentManager`

### Scope

`Document::setType` currently reaches back into
`m_ctx.getDocumentManager().submitIntellisense(this, …)` and
`m_ctx.getSideBarManager().showSymbolsFor(nullptr)`. That makes
Document depend on the manager tree for side effects — couples the
model to consumers.

Replace with an observer pattern:

```cpp
// In Document.hpp:
using TypeChangedHandler = std::function<void(Document&, DocumentType prev)>;
void onTypeChanged(TypeChangedHandler handler);

// In DocumentManager (one-time, in ctor or per-new-doc):
doc.onTypeChanged([this](Document& d, DocumentType prev) {
    if (d.getType() == DocumentType::FreeBASIC) {
        submitIntellisense(&d, d.getEditor()->GetText());
    } else {
        cancelIntellisense(&d);
        if (getActive() == &d) {
            m_ctx.getSideBarManager().showSymbolsFor(nullptr);
        }
    }
});
```

Or simpler: a single `Document::Observer` interface with a couple of
methods, registered once with the document at attach time. Pick
whichever reads cleaner once we have the call sites in front of us.

### Files

- Modify: `document/Document.{hpp,cpp}` (drop direct manager calls),
  `document/DocumentManager.cpp` (subscribe to events at
  construction).

### Risks

Medium. Defines the "Document is the model; manager observes" pattern
that the rest of project work will reuse. Get the API shape right —
worth one round of API review before locking in.

### TDD

Yes — the observer wiring is testable. Construct a Document, register
a counting handler, call `setType`, assert handler fired with
expected args. Doesn't need any wx widgets.

---

## Phase 4 — `ProjectNode` back-link stub

### Scope

Forward declare `class ProjectNode;` in `document/Document.hpp`. Add:

```cpp
[[nodiscard]] auto projectNode() const -> ProjectNode* { return m_projectNode; }
void setProjectNode(Unowned<ProjectNode> node) { m_projectNode = node; }

private:
Unowned<ProjectNode> m_projectNode = nullptr;
```

No implementation behind it. Future project code creates `ProjectNode`s
and links them to Documents.

### Files

- Modify: `document/Document.hpp` only.

### Risks

None. Pure type addition.

### TDD

Trivial — `EXPECT_EQ(nullptr, doc.projectNode());
doc.setProjectNode(node); EXPECT_EQ(node, doc.projectNode());` —
included in DocumentTests.cpp once it exists.

---

## Critical review of the plan

### What's good

- Each phase ships independently. Phase 1 is a pure encapsulation
  move; phase 2 the breakthrough that unlocks testing; phase 3 the
  decoupling that future project code will piggyback on; phase 4 a
  cheap stub.
- The text-buffer decision is made up front (lives in view) — no
  second-guessing during implementation.
- Concrete `EditorPanel` skips speculative abstraction. The
  abstract-base introduction (if/when needed) happens once we have
  two real cases.
- Phase 2 unlocks **real unit tests** for the document layer for the
  first time — adds a fixture once, then every future doc-layer change
  has a TDD surface.

### Trade-offs accepted

- **Text discarded when tab closes** — matches today's behaviour but
  may surprise users who expect their unsaved buffer to survive
  closing a project file. Modified prompt still fires (it's mandatory
  for unsaved buffers), so accidents are caught.
- **Document holds a concrete `EditorPanel*` back-link.** When the
  image viewer arrives we'll need to revisit. Acceptable cost for not
  pre-abstracting now.
- **Phase 3's observer API** is the most speculative — we don't yet
  know all the events project code will want. Land it small (just the
  setType case), let project work expand as needed.

### Risks I'm NOT mitigating up front

- **Edit-time coupling of `Editor`.** Editor itself takes a fistful
  of dependencies (`DocumentManager*`, `UIManager*`,
  `CodeTransformer*`, …). EditorPanel just forwards these through.
  If we ever want `EditorPanel` testable in isolation, Editor itself
  needs a separate slim-down pass. Out of scope here.
- **Multiple views per Document** (split editor, second pane). Not in
  scope. The `Unowned<EditorPanel> m_panel` slot is single-valued. If
  multi-view becomes a goal, the back-link grows to a small
  collection.

### What I considered and rejected

- **Abstract `DocumentView` base now.** Speculative until a second
  concrete view exists. Add later if/when needed.
- **Text buffer on Document, view-syncs.** Two buffers to keep
  coherent, complex sync logic. Real cost; no real benefit until we
  need view-less doc editing (which we don't).
- **Phase 3 as event-bus / publisher-subscriber framework.** YAGNI —
  one event type so far. Use a `std::function` or small Observer
  interface, scale up only when more events show up.

---

## TODO sequence

Each phase = one commit, with clang-tidy on touched code + tests +
smoke before commit. Phase 2 also adds DocumentTests.cpp.

1. **Phase 1** — Introduce `EditorPanel`; encapsulate view ownership.
2. **Phase 2** — Document constructible without view + attach/detach
   API + first DocumentTests.cpp.
3. **Phase 3** — Replace `Document → DocumentManager` direct calls in
   `setType` with an observer hook.
4. **Phase 4** — Add `ProjectNode` back-link stub.

After phase 4: Document is a true model object — constructible without
wx, testable in isolation, with named back-link slots for the future
project layer to fill in.
