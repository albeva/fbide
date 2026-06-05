# Project File Format — Specification

## Goal

Persist a Persistent project's tree to disk so it survives a restart and
travels between machines via `git clone`. Two sibling files at the project
root: one **committed** (`.fbp`) and one **per-user** (`.fbide/session.ini`).

## File layout

```
project-root/
├── snake.fbp                  # committed: project structure
├── .fbide/                    # IDE-state directory (users own their .gitignore)
│   └── session.ini            # auto: per-user UI state
├── src/
├── docs/
└── inc/
```

The **project root** is the directory containing `.fbp`. All file and
folder paths in the project tree must sit under the project root —
enforced by `Project::isUnderRoot()` and validated at every mutation
that takes a path (`addFile`, `addRealFolder`, `setFilePath`).

## `.fbp` — project structure

INI via `wxFileConfig`. Committed to git. Stable across IDE sessions
and machines. Schema-versioned for future migrations.

### Top-level

```ini
format=1                   ; schema version (bump on incompatible changes)
version=0.6.0              ; fbide version that last wrote the file
name=snake game            ; project display name (distinct from root folder name)
```

- `format` — schema version. Always present. Bumped only when a
  load-incompatible change is made.
- `version` — fbide version footprint, matches the `version=` field
  in `themes` / `config` files. Informational.
- `name` — project's display name. Distinct from the root folder's
  display name (which mirrors the on-disk dir basename).

### Folder summary

`<id>=<basename>`. Lists every folder in the tree except the implicit
root. `<id>` is the node's `Node::Id` — a short base-62 string (see
`utils/Identifier.hpp`). Each entry is a key under the `[folders]` group,
iterable via `wxFileConfig::GetFirstEntry`.

```ini
[folders]
aZ3kP9q=src
b7Kt2mX=docs
c1Qf8Lp=inc
```

### File summary

`<id>=<basename>`. Lists every file in the tree.

```ini
[files]
d4Rb9nW=hello.bas
e8Tz3kP=world.bas
f2Yh7Qm=main.bas
g6Lc1Xs=README.md
```

### Per-folder detail

Only emit when the folder has non-default fields. Top-level folders
omit `parent` (implicit root parent). Real folders omit `virtual`
(real is the default).

```ini
[folders/c1Qf8Lp]
parent=aZ3kP9q             ; omit for root-level folders
virtual=1                  ; 0 is default; only write when virtual
```

### Per-file detail

Only emit fields that diverge from `ConfigManager` defaults or detected
type. The project file is the single source of truth for these
per-file values.

```ini
[files/d4Rb9nW]
parent=aZ3kP9q
encoding=UTF-8             ; omit when matches editor.encoding
eol=LF                     ; omit when matches editor.eolMode
type=freebasic             ; omit when matches detected-from-extension
```

### Reserved (future) — `[build]` section

Deferred for a separate design pass. Will cover: output kind
(Executable / Library / StaticLib), build directory, artefact paths,
executable to launch on Run, compile/run command overrides,
pre/post hooks.

## Disk path composition — recursive backtrack

A file's on-disk path is **derived**, not stored. The algorithm:

1. Start with the file's `basename` (from the `[files]` summary).
2. Walk up `parent` links.
3. For each ancestor:
   - **Real folder** (path on disk): join its disk path onto the basename
     under construction. Stop.
   - **Virtual folder**: skip — virtual folders are display-only and
     don't contribute to disk layout. Continue walking up.
4. If the walk reaches the project root without hitting a real folder,
   the file sits directly at the root: `<root>/<basename>`.

Result: virtual folders are transparent to disk layout. Real files
can be grouped under virtual folders for organisation without
affecting their on-disk locations. The same algorithm `moveNode`
already uses to find the closest real ancestor for auto-`fs::rename`.

## `.fbide/session.ini` — per-user runtime state

Auto-managed sidecar to the `.fbp`. Loaded when the project is opened;
saved when the project is closed (manual Close Project, switching to
another project, or app exit). Distinct from the standalone `.fbs`
session — `.fbs` is user-driven and standalone-meaningful, this file is
meaningless without its sibling `.fbp`.

Per-document attributes are written / read by
`Document::setSessionAttributes` / `loadSessionAttributes` — the same code
the `.fbs` session uses.

```ini
[session]
version=1
open=d4Rb9nW,e8Tz3kP       ; open documents, in tab order (Node::Ids)
activeDocument=d4Rb9nW     ; focused editor tab; empty if none
selectedNode=c1Qf8Lp       ; project-tree selection (file or folder); empty if none
expanded=aZ3kP9q,c1Qf8Lp   ; expanded folder nodes

[files/d4Rb9nW]
scroll=10
cursor=250
encoding=UTF-8             ; always written
eolMode=LF                 ; always written
type=freebasic            ; only when the user overrode the detected type
configuration=cfg-2        ; only when pinned to a non-active compiler config
folds=12,34                ; collapsed lines, only when the fold margin is on
```

- Keyed by `Project::Node::Id` (a short base-62 string). No paths; the
  project file is the source of truth for tree structure.
- On load the listed `open` documents are reopened (each restoring its
  editor state), the `activeDocument` tab is focused, and the tree's
  expanded folders + selected node are restored.
- Untitled or non-project files are never in this file (they belong to
  the standalone `.fbs` session flow).

## Implementation API — proposed

Member functions on `Project`:

```cpp
class Project {
public:
    // Project display name (top-level `name=` in .fbp). Distinct from
    // the root folder's display name.
    [[nodiscard]] auto getName() const -> const std::string&;
    void setName(std::string name);

    // Save the tree to `projectFile` (writes the .fbp). Caller must
    // already have ensured projectFile.parent_path() matches the
    // project's root, or have called setProjectRoot() first. Does
    // not touch the session file — WorkspaceManager owns that side.
    [[nodiscard]] auto saveTo(const std::filesystem::path& projectFile) const
        -> std::expected<void, Error>;

    // Construct a Persistent project from a .fbp. Project root is
    // set to projectFile.parent_path(). The session file (if any) is
    // NOT loaded by this call — WorkspaceManager pairs the two.
    [[nodiscard]] static auto loadFrom(
        const std::filesystem::path& projectFile,
        ConfigManager& config
    ) -> std::expected<std::unique_ptr<Project>, Error>;
};
```

Errors added to `Project::Error`:

- `FormatError` — `.fbp` couldn't be parsed (malformed INI, missing
  required keys, dangling parent reference).
- `IoError` — read/write failed (already exists).
- `Error::Clash` / `Error::InvalidName` / `Error::OutOfTree` — unchanged.

## Implementation split

Three commits, each independently testable:

### 3A — `.fbp` round-trip

- `Project::m_name` field + getter/setter.
- `saveTo` / `loadFrom` for the project file.
- `Project::Error::FormatError`.
- Tests: TempDir + save + reload + assert tree shape preserved.
- No session, no WorkspaceManager hookup yet.

### 3B — Per-project session

- Save/load `.fbide/session.ini` (UI state only).
- Hooked into the Document / Editor layer to read folds / scroll /
  cursor at save time and apply them at load time.
- Decision deferred: extend `FileSession` or new helper class.

### 3C — `WorkspaceManager` open/close

- `WorkspaceManager::openProject(path)` / `closeProject(project)`.
- UI command + recent-project list.
- Auto-pairs `.fbp` + `.fbide/session.ini` on open/save.

## Open questions

- Where do `saveTo` / `loadFrom` ultimately live — `Project` members
  (current proposal) or a new `ProjectIO` class? Punted to when 3A
  lands; revisit if the IO grows.
- `.fbide/` may host more than session state over time (build cache,
  per-user notes, etc.). Convention is established now; specific
  files come as needed.
- `[build]` section needs its own design pass — at minimum:
  output kind, build path, artefact path, executable path,
  pre/post-build/run hooks, compile/run command overrides.
- Cross-platform path encoding: emit all relative paths via
  `lexically_relative(path, root).generic_string()` so forward
  slashes are written on every platform.
