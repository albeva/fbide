# Project Refactor — TODO Breakdown

Derived from [`project-refactor.md`](./project-refactor.md). Seven phases. Each
lands as its own commit; existing tests and behaviour must pass at every phase
boundary.

---

## Phase 1 — Project / Node skeleton

**Goal:** Type-level scaffolding only. No callers, no integration.

**Prerequisite:** none.

- [ ] Create module directory: `src/lib/workspace/` (houses both `Project` and
      `WorkspaceManager` long term).
- [ ] Add module to `CMakeLists.txt`.
- [ ] Define `Project::Node::Id` opaque type (header + `std::hash`
      specialisation), modelled on lbc's `DiagId`.
- [ ] Define `Project::Mode` enum (`Ephemeral`, `Persistent`).
- [ ] Define `Project::FileEntry`, `Project::FolderEntry`, `Project::Node`
      structs.
- [ ] `Project` class skeleton with:
  - Constructor `(Context&, Mode)`.
  - `getMode()`, `isEphemeral()`.
  - `addFile(std::optional<path>, Document*) -> Node::Id`.
  - `getNodePath(Node::Id) const -> std::filesystem::path` (by value).
  - `setNodePath(Node::Id, const std::filesystem::path&)` — updates
    `FileEntry::path` and re-keys `m_byPath`.
  - `getPrimarySource() -> Document*` — defined for `Ephemeral` only (assert
    on `Persistent`).
  - `getDocuments() const -> std::vector<Document*>` (or span) — iterates file
    nodes with non-null `doc`.
  - `getId() const -> ProjectId` (separate opaque ID type for the project
    itself; also `std::size_t`-backed).
  - Internal: `m_nodes`, `m_byPath`, `m_root`, monotonic `Node::Id` allocator,
    monotonic `ProjectId` allocator.

**Exit criteria:** `cmake --build` succeeds. `tests/tests` still pass. No call
sites yet.

---

## Phase 2 — WorkspaceManager skeleton

**Goal:** Exists in `Context`, all entry points are stubs.

**Prerequisite:** Phase 1.

- [ ] `WorkspaceManager` class (header + cpp) in `src/lib/workspace/`.
- [ ] Add to `Context`:
  - Declare **after** `DocumentManager` so destruction order is
    `WorkspaceManager` first (matters for Phase 3 when `IntellisenseService`
    moves in — worker must join before documents go away).
  - Accessor `getWorkspaceManager()` + const overload.
  - Construct in `Context` ctor.
- [ ] Stub methods:
  - `getActiveProject()` returns `nullptr`.
  - `setActiveDocument(Document*)` is a no-op.
  - `createEphemeral`, `destroyEphemeral`, `closeProject` — assert-only /
    unreachable for now.
  - `resolveOrOpen(path)` forwards to `DocumentManager::openFile`.
  - `contains(Project*)` returns false.
- [ ] Add `std::unordered_map<ProjectId, std::unique_ptr<Project>> m_projects`
      (empty).

**Exit criteria:** App builds and launches. Behaviour unchanged.

---

## Phase 3 — IntellisenseService moves to WorkspaceManager

**Goal:** Lifetime ownership shifts; `DocumentManager` forwards.

**Prerequisite:** Phase 2.

- [ ] Move `std::unique_ptr<IntellisenseService> m_intellisense` from
      `DocumentManager` to `WorkspaceManager`.
- [ ] Move construction (was in `DocumentManager` ctor) to `WorkspaceManager`
      ctor.
- [ ] `IntellisenseService` sink stays the `DocumentManager` (it handles
      `EVT_INTELLISENSE_RESULT`) — pass `&docManager` from `WorkspaceManager`
      ctor (requires `Context` chicken-and-egg consideration: construct order
      is `DocumentManager` → `WorkspaceManager`, so `WorkspaceManager` ctor can
      dereference `m_documentManager`).
- [ ] Add `WorkspaceManager::getIntellisense() -> IntellisenseService&`.
- [ ] `DocumentManager::submitIntellisense` forwards through
      `m_ctx.getWorkspaceManager().getIntellisense().submit(...)`.
- [ ] `DocumentManager::cancelIntellisense` same forward pattern.
- [ ] `DocumentManager::closeFile`'s `prune()` call routes through
      `WorkspaceManager`.
- [ ] Remove the field-order comment in `DocumentManager.hpp` about
      `m_intellisense` being last.
- [ ] Audit destruction-order risk: confirm `WorkspaceManager` destroyed
      before `DocumentManager` (worker joins before documents disappear).

**Exit criteria:** Open a FB file, symbol browser populates. Build a project,
no use-after-free at shutdown. `tests/tests` pass.

---

## Phase 4 — `Document::m_source` variant (riskiest commit)

**Goal:** Path storage migrated to variant; project pointer added; project arm
not exercised yet.

**Prerequisite:** Phase 1.

- [ ] In `Document.hpp`:
  - Forward declare `Project`.
  - Define `using Source = std::variant<std::filesystem::path, Project::Node::Id>`.
  - Replace `std::filesystem::path m_filePath` with `Source m_source`.
  - Remove `Unowned<ProjectNode> m_projectNode`; replace with
    `Project* m_project = nullptr` (also retires the pre-existing
    `Unowned`-of-non-wx-pointer inconsistency).
  - Remove the `class ProjectNode;` forward declaration.
- [ ] Add member functions:
  - `getSource() const -> const Source&`.
  - `getProject() const -> Project*`.
  - `bindToProject(Project&, Project::Node::Id)`.
  - `unbindFromProject()` — copies path out of project before clearing pointer.
- [ ] Change `getFilePath()` to return `std::filesystem::path` by value;
      dispatch on variant.
- [ ] Update `setFilePath()` to dispatch (path arm: direct assignment; node
      arm: `m_project->setNodePath(...)`).
- [ ] Update `isNew()` to test the variant correctly (path arm: empty path;
      node arm: query project for `FileEntry::path.has_value()`).
- [ ] Update method bodies in `Document.cpp` that read `m_filePath` directly:
      `getFrameTitle`, `getTitle`, `checkExternalChange`, `updateModTime`,
      `setFilePath`.
- [ ] **Audit all `getFilePath()` callers** across the codebase (mechanical
      sweep):
  - Anything binding `const auto&` silently becomes a copy — check each is
    fine.
  - Anything storing `const std::filesystem::path&` long-term needs review.
- [ ] Remove leftover `class ProjectNode;` references; replace with
      `class Project;` forward decls if used.

**Exit criteria:** Open / save / save-as / reload / close all work.
`tests/tests` pass. App launches, opens a file from CLI, saves cleanly.

---

## Phase 5 — Ephemeral lifecycle wiring

**Goal:** Every FB document is project-bound. Lifecycle is observable
end-to-end.

**Prerequisite:** Phase 2 + Phase 4.

- [ ] Implement (no longer stubs): `WorkspaceManager::createEphemeral`,
      `destroyEphemeral`, `closeProject`, `setActiveDocument`,
      `getActiveProject`, `contains(Project*)`.
- [ ] `WorkspaceManager` subscribes to `EVT_DOCUMENT_TYPE_CHANGED` in its
      ctor; implement `onDocumentTypeChanged`:
  - `new == FB && doc.getProject() == nullptr` → `createEphemeral(doc)`.
  - `new != FB && doc.getProject() != nullptr && doc.getProject()->isEphemeral()`
    → `destroyEphemeral(doc)`.
- [ ] `DocumentManager::newFile(FreeBASIC)` → call `createEphemeral` after
      document construction.
- [ ] `DocumentManager::openFile(path)` → call `createEphemeral` after
      `documentTypeFromPath` resolves to `FreeBASIC` (after `markSaved` /
      `setFilePath`).
- [ ] `DocumentManager::closeFile` → before erasing, if doc has ephemeral
      project, `destroyEphemeral`.
- [ ] `DocumentManager` calls `WorkspaceManager::setActiveDocument(doc)` from
      wherever active-tab change is currently observed.
- [ ] Verify ordering invariant: `createEphemeral` is only called when
      `doc.getType() == FreeBASIC`. Confirm no path where it would be called
      too early (e.g. before `setType` settles in `openFile`).
- [ ] Manual verification:
  - New untitled FB doc → has project, `getActiveProject() != nullptr`.
  - Open `.bas` → has project; switch tab away → active project becomes
    nullptr or other project.
  - Status-bar type override FB → HTML → project destroyed (verify via temp
    logging or breakpoint).
  - HTML → FB → fresh ephemeral project created.
  - Close tab → project destroyed.

**Exit criteria:** All above manual checks pass. `tests/tests` pass. No
project leaks at shutdown (visible via debug allocator or temp logging if
needed).

---

## Phase 6 — Compile/run reads from Project

**Goal:** `m_compiledFile` and compile options live on `Project`;
`CompilerManager` orchestrates around it.

**Prerequisite:** Phase 5.

- [ ] Add `Project::m_compiledFile` + `getCompiledFile()` / `setCompiledFile()`.
- [ ] Add compile-options accessors on `Project` — for `Ephemeral`, forward to
      `ConfigManager` at call time. List the accessors needed by
      `CompileCommand` / `RunCommand` / `BuildTask`; mirror their current
      `ConfigManager` reads.
- [ ] `BuildTask`:
  - Replace `Document* m_doc` with `Project* m_project`.
  - Constructor takes `Project*`.
  - `getDocument()` removed; introduce `getProject()` (validated via
    `WorkspaceManager::contains`).
  - `onCompileFinished` writes through `m_project->setCompiledFile(...)`
    instead of `Document::setCompiledPath`.
  - `onRunFinished`'s editor focus fix-up walks
    `project->getPrimarySource()->getEditor()`.
- [ ] `CompilerManager`:
  - Private `getActiveProject()` replaces `getActiveDocument()`; null when
    active doc has no project (covers `type != FreeBASIC` for ephemeral).
  - `compile`, `compileAndRun`, `run`, `quickRun` all switch to project-driven.
  - `ensureSaved` takes a `Project&` and walks its source files (for
    ephemeral, this is the single primary source; same behaviour as today).
- [ ] Remove from `Document`: `m_compiledFile`, `getCompiledFile`,
      `setCompiledPath`. Grep for stragglers.
- [ ] Sanity-check anywhere else in the code that read
      `Document::getCompiledFile` — none should remain.

**Exit criteria:** Compile, run, compile-and-run, quick-run all work with a
FB file. Error parsing still navigates correctly. `tests/tests` pass.

---

## Phase 7 — `resolveOrOpen` + `goToError` rewire

**Goal:** File-resolution intermediary in place; future-proofs for persistent
projects.

**Prerequisite:** Phase 6.

- [ ] Implement `WorkspaceManager::resolveOrOpen(const std::filesystem::path&) -> Document*`:
  - Rule 1: `DocumentManager::findByPath` hit → return it.
  - Rule 2: stub comment / TODO for persistent project lookup.
  - Rule 3: `DocumentManager::openFile(path)`.
- [ ] `CompilerManager::goToError`:
  - Temp file branch: `m_task->getProject()->getPrimarySource()`.
  - Other branches:
    `m_ctx.getWorkspaceManager().resolveOrOpen(toFsPath(fileName))`.
- [ ] Remove now-unused direct `DocumentManager::openFile` call from
      `goToError`.

**Exit criteria:** Trigger compile error in a saved file → click navigates.
Quick-run with an error → click navigates back to the source buffer
(FBIDETEMP mapping works). `tests/tests` pass.

---

## Cross-phase notes

- **Verification cadence:** after each phase, run `tests/tests`, then a manual
  smoke: launch app → new file → type FB code → run. If symbol browser
  populates and execution completes, the phase is healthy.
- **No new tests required for refactor correctness**, but a couple of small
  unit tests around `Project::addFile` / `getNodePath` / `setNodePath`
  (Phase 1) and `WorkspaceManager::createEphemeral` / `destroyEphemeral`
  (Phase 5) would catch regressions cheaply and document intent. Optional,
  decide when we get there.
- **Rollback boundary:** Phase 4 is the only one with wide blast radius. Land
  it isolated; if it regresses something subtle, revert is one commit.
- **CLAUDE.md:** no update needed during the refactor; one summarising line
  about the workspace module after Phase 7 is enough.
