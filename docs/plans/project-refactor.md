# Project Refactor — Implementation Plan

## Goal

Introduce a `Project` abstraction as the foundation for future multi-file
project support, without disrupting the current "new tab → type → run" loop.
This phase is **pure internal refactor** — no UI surface, only Ephemeral
projects, behaviour preserved.

## Component Responsibilities

| Component              | Owns                                                                                  | Role                                                                                                                   |
|------------------------|---------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------|
| **WorkspaceManager**   | All open `Project` instances, shared `IntellisenseService`                            | Tracks active project; lifecycle entry points (`createEphemeral`, `destroyEphemeral`, `closeProject`, `resolveOrOpen`) |
| **Project**            | File/folder tree, compile options, last `m_compiledFile`, compile/run command strings | Single source of truth for build inputs; reads from `ConfigManager` when ephemeral                                     |
| **Document**           | Buffer state, encoding/EOL, view back-link, `m_source` variant, `Project*` back-link  | One open file; binding to a project via `bindToProject` / `unbindFromProject`                                          |
| **DocumentManager**    | `unique_ptr<Document>` per open doc, notebook, transformer                            | Open/save/close pipeline; calls `WorkspaceManager` for project lifecycle                                               |
| **CompilerManager**    | In-flight `BuildTask`                                                                 | Orchestrates compile/run by *reading* the active `Project`; never reads `ConfigManager` for build options              |
| **SymbolTable**        | (unchanged)                                                                           | Per-document parse result; no project back-link                                                                        |

## Pointer convention

`Unowned<T>` is reserved for the `new`-allocated, wx-parented case (silences
clang-tidy on raw `new`). For non-owning back-links to objects held in
`unique_ptr` elsewhere, use plain `T*`. The wx-managed back-links in `Document`
(`m_view`, `m_editor`) stay `Unowned`; the project pointer is plain `Project*`.

## Data Model

### `Project::Node::Id` — opaque strong type

Modelled on lbc's `DiagId` idiom:

```cpp
class Id {
public:
    using Underlying = std::size_t;
    constexpr Id() = default;
    constexpr explicit Id(Underlying v) : m_v(v) {}
    constexpr auto value() const -> Underlying { return m_v; }
    constexpr auto operator<=>(const Id&) const = default;
    explicit constexpr operator bool() const { return m_v != 0; }
private:
    Underlying m_v = 0;
};
// + std::hash specialization
```

Zero = invalid sentinel.

### Project node tree

Flat arena keyed by `Node::Id`; children stored as IDs, not pointers:

```cpp
struct FileEntry {
    std::optional<std::filesystem::path> path;  // nullopt for untitled
    Document* doc = nullptr;                    // null when closed
};
struct FolderEntry {
    std::optional<std::filesystem::path> path;  // nullopt for virtual folder
    std::string name;
    std::vector<Node::Id> children;
};
struct Node {
    Node::Id id;
    Node::Id parent;                            // invalid for root
    std::variant<FileEntry, FolderEntry> entry;
};

std::unordered_map<Node::Id, Node>                  m_nodes;    // single source of truth
std::unordered_map<std::filesystem::path, Node::Id> m_byPath;   // lookup index
Node::Id m_root;
```

No `m_documentToId` — reach via `Document::getProject()` + `Document::getSource()`.

### Project

```cpp
enum class Mode : std::uint8_t { Ephemeral, Persistent };

class Project {
    Context& m_ctx;
    Mode m_mode;
    // Tree as above
    wxString m_compiledFile;        // moved from Document
    // Compile options: for Ephemeral, accessors forward to ConfigManager;
    // for Persistent (future), stored here.
};
```

**Compile options accessors**: every getter for ephemeral reads through
`ConfigManager` at call time (preserves today's "settings change → next build
picks it up" behaviour). Persistent projects later store their own.

**For ephemeral specifically**: `Project::getPrimarySource() -> Document*` —
defined only when `mode == Ephemeral`; returns the doc bound to the single
file node. Used by `CompilerManager::goToError` for the `FBIDETEMP.BAS`
special case.

### Document changes

```cpp
using Source = std::variant<std::filesystem::path, Project::Node::Id>;

class Document {
    Source     m_source;            // replaces m_filePath
    Project*   m_project = nullptr; // non-null iff m_source holds Node::Id
    // ... rest unchanged
};
```

**Invariant**: `m_project != nullptr` iff `m_source` holds `Node::Id`. Enforced
by paired atomic transitions:

```cpp
void Document::bindToProject(Project& p, Project::Node::Id id) {
    m_project = &p;
    m_source = id;
}

void Document::unbindFromProject() {
    if (m_project != nullptr && std::holds_alternative<Project::Node::Id>(m_source)) {
        const auto id = std::get<Project::Node::Id>(m_source);
        m_source = m_project->getNodePath(id);   // copy path out before unlink
    }
    m_project = nullptr;
}
```

**Path accessors**:

```cpp
auto getFilePath() const -> std::filesystem::path {    // by-value
    if (std::holds_alternative<std::filesystem::path>(m_source)) {
        return std::get<std::filesystem::path>(m_source);
    }
    return m_project->getNodePath(std::get<Project::Node::Id>(m_source));
}

void setFilePath(const std::filesystem::path& p) {
    if (std::holds_alternative<std::filesystem::path>(m_source)) {
        std::get<std::filesystem::path>(m_source) = p;
    } else {
        m_project->setNodePath(std::get<Project::Node::Id>(m_source), p);
    }
}
```

`Project::setNodePath` updates `FileEntry::path` and re-keys `m_byPath`.

**`getFilePath()` migration**: today returns `const std::filesystem::path&`.
By-value change is mechanical across all callers; budgeted into step 4.

## WorkspaceManager API (initial)

```cpp
class WorkspaceManager final {
public:
    auto createEphemeral(Document& doc) -> Project&;
    void destroyEphemeral(Document& doc);
    void closeProject(Project& p);
    auto resolveOrOpen(const std::filesystem::path& path) -> Document*;

    auto getActiveProject() -> Project*;        // null when active doc has no project
    auto getIntellisense() -> IntellisenseService&;

    // Internal — invoked when active tab changes
    void setActiveDocument(Document* doc);

private:
    Context& m_ctx;
    std::unordered_map<ProjectId, std::unique_ptr<Project>> m_projects;
    std::unique_ptr<IntellisenseService> m_intellisense;
    Project* m_activeProject = nullptr;
};
```

### `createEphemeral` body

```cpp
auto WorkspaceManager::createEphemeral(Document& doc) -> Project& {
    assert(doc.getProject() == nullptr);
    assert(doc.getType() == DocumentType::FreeBASIC);

    // Read path out of doc BEFORE bindToProject overwrites the variant.
    std::optional<std::filesystem::path> path;
    if (const auto& src = doc.getSource();
        std::holds_alternative<std::filesystem::path>(src)) {
        if (const auto& p = std::get<std::filesystem::path>(src); !p.empty()) {
            path = p;
        }
    }

    auto project = std::make_unique<Project>(m_ctx, Project::Mode::Ephemeral);
    const auto nodeId = project->addFile(path, &doc);
    doc.bindToProject(*project, nodeId);

    auto& ref = *project;
    m_projects.emplace(ref.getId(), std::move(project));
    return ref;
}
```

### `destroyEphemeral` / `closeProject`

```cpp
void WorkspaceManager::destroyEphemeral(Document& doc) {
    auto* p = doc.getProject();
    assert(p != nullptr && p->isEphemeral());
    doc.unbindFromProject();
    closeProject(*p);
}

void WorkspaceManager::closeProject(Project& project) {
    // Skip docs whose back-link has already drifted away — they were
    // unbound out-of-band and are not this project's responsibility.
    for (auto* document : project.getDocuments()) {
        if (document->getView() != nullptr && document->getProject() == &project) {
            document->unbindFromProject();
            m_ctx.getDocumentManager().closeFile(*document);
        }
    }
    m_projects.erase(project.getId());
}
```

### `resolveOrOpen`

```cpp
auto WorkspaceManager::resolveOrOpen(const std::filesystem::path& path) -> Document* {
    // 1. Already open by path → return existing doc.
    // 2. (future) File is a member of any open Persistent project → open + bind to it.
    // 3. Otherwise → DocumentManager::openFile, which will trigger createEphemeral
    //    via the standard FB-file-loaded code path.
}
```

In this phase only rule 3 fires; the hook is in the right place for Persistent
later.

## Lifecycle Rules

### Ephemeral creation — direct calls, no new events

Three sites in `DocumentManager`:

1. **`newFile(DocumentType::FreeBASIC)`** — after constructing the doc, call
   `m_ctx.getWorkspaceManager().createEphemeral(doc)`.
2. **`openFile(path)`** — after `documentTypeFromPath` resolves to `FreeBASIC`
   (DocumentManager.cpp:177), same call.
3. **`onDocumentTypeChanged`** handler — see below.

### Type-change handling — `WorkspaceManager` joins as second subscriber

`EVT_DOCUMENT_TYPE_CHANGED` already exists with `DocumentManager::onDocumentTypeChanged`
as subscriber. `WorkspaceManager` subscribes alongside (does not displace it):

- `DocumentManager::onDocumentTypeChanged` continues to handle intellisense
  submit/cancel and sidebar refresh.
- `WorkspaceManager::onDocumentTypeChanged`:
  - `new == FreeBASIC && doc.getProject() == nullptr` → `createEphemeral(doc)`.
  - `new != FreeBASIC && doc.getProject() != nullptr && doc.getProject()->isEphemeral()`
    → `destroyEphemeral(doc)`.
  - Persistent projects: untouched.

### Ephemeral destruction — single named transition

`DocumentManager::closeFile` (and the closing-app sweep) checks:

```cpp
if (auto* p = document->getProject(); p != nullptr && p->isEphemeral()) {
    m_ctx.getWorkspaceManager().destroyEphemeral(*document);
}
```

### Active project tracking

`WorkspaceManager::setActiveDocument(Document*)` invoked from `DocumentManager`
on tab change. Active project is `doc->getProject()` (may be null). No event
published yet — no subscribers in this phase.

### Intellisense pool prune

`DocumentManager::closeFile` calls
`m_ctx.getWorkspaceManager().getIntellisense().prune()` at the same point it
does today.

## Compile / Run Flow

`CompilerManager` reads `WorkspaceManager::getActiveProject()` instead of
`DocumentManager::getActive()`:

```cpp
void CompilerManager::compile() {
    auto* project = m_ctx.getWorkspaceManager().getActiveProject();
    if (project == nullptr || !ensureSaved(*project)) return;
    m_task = std::make_unique<BuildTask>(m_ctx, project);
    m_task->compile(project->getPrimarySourcePath());
}
```

`BuildTask` holds `Project*`, validates liveness via
`WorkspaceManager::contains(Project*)` instead of
`DocumentManager::contains(Document*)`.

`m_compiledFile` writes go to `Project::setCompiledFile`, not
`Document::setCompiledPath`.

`goToError` keeps the temp-file special case, but routes through Project:

```cpp
if (isTemp && m_task->isQuickRun()) {
    return m_task->getProject()->getPrimarySource();
}
return m_ctx.getWorkspaceManager().resolveOrOpen(toFsPath(fileName));
```

## Out-of-scope (this phase)

- Persistent projects — `Project::Mode::Persistent` enum value exists, no
  implementation behind it.
- Project file format (`.fbp` INI, decided but not implemented).
- Project explorer UI / sidebar surface.
- Multi-project workspace, cross-project intellisense, inter-project relations.
- Save-As of a project-bound document mutating `FileEntry::path` + `m_byPath` —
  only needs to *work* (path updates correctly via `setFilePath` →
  `Project::setNodePath`), no UI for renaming via the tree.

## Sequencing

Each step compiles cleanly and preserves behaviour. Land one at a time.

1. **NodeId + Project / Node skeleton** — types, no behaviour wired. Ephemeral
   mode only. `Project::addFile`, `getNodePath`, `setNodePath`,
   `getPrimarySource`, `getDocuments`, `getId`, `isEphemeral`.
2. **WorkspaceManager skeleton** — added to `Context` after `DocumentManager`;
   no subscriptions, no projects yet. Just `getActiveProject() == nullptr`
   always.
3. **Move `IntellisenseService` → `WorkspaceManager`**.
   `DocumentManager::submitIntellisense` / `cancelIntellisense` forward to
   `WorkspaceManager::getIntellisense()`. `DocumentManager::closeFile`'s
   `prune()` call routes through `WorkspaceManager`.
4. **`Document::m_filePath` → `Document::m_source` variant** (only `path` arm
   exercised — no projects yet). By-value `getFilePath()`. Mechanical caller
   migration. Add `bindToProject` / `unbindFromProject` (unused yet). Also
   replaces the existing `Unowned<ProjectNode> m_projectNode` field (pre-existing
   inconsistent use of `Unowned` for a non-wx pointer — gets cleaned up here).
5. **Wire ephemeral lifecycle** — `WorkspaceManager` subscribes to
   `EVT_DOCUMENT_TYPE_CHANGED`; `DocumentManager::newFile` / `openFile` call
   `createEphemeral`; `closeFile` calls `destroyEphemeral`; `setActiveDocument`
   updates active project. After this step, every FB doc is project-bound.
6. **Move `m_compiledFile` + compile options to `Project`**; switch `BuildTask`
   to `Project*`; `CompilerManager::getActiveDocument()` → `getActiveProject()`.
   `Document::m_compiledFile` / `getCompiledFile` / `setCompiledPath` removed.
7. **`WorkspaceManager::resolveOrOpen`** + rewire `CompilerManager::goToError`
   through it.

Step 4 is the riskiest single commit (wide blast radius from the path-API
change); landing it on its own with no project plumbing makes review tractable.
