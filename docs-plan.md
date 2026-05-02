# FBIde Documentation Plan

Working document for a coordinated Doxygen pass over the codebase. From
this plan we derive individual TODOs.

## 1. Goals

- Generate a navigable Doxygen site (HTML, dark theme, dot-driven graphs).
- Document every class, struct, enum and free function in `src/`.
- Document private *and* public methods (we own the codebase end-to-end).
- Add long-form architecture / subsystem pages so the docs aren't just an
  API reference — they explain how the pieces fit.
- Pass clang-format over the entire tree once everything is in place.
- Style: clear, concise, "why over what"; no padding for word count.

## 2. Tooling

### 2.1 CMake integration

Mirror lbc's setup:

```
cmake/doxygen.cmake     # find_package(Doxygen) + add_custom_target(docs)
Doxyfile.in             # configured by CMake
doxygen/                # markdown subpage sources
docs/                   # generated output (gitignored)
```

`docs` target: `cmake --build build/claude/debug --target docs`.

### 2.2 Doxyfile.in baseline

```
PROJECT_NAME           = "FBIde"
PROJECT_NUMBER         = "@CMAKE_PROJECT_VERSION@"
PROJECT_BRIEF          = "Open-source IDE for FreeBASIC"
OUTPUT_DIRECTORY       = "@CMAKE_SOURCE_DIR@/docs"

INPUT                  = @CMAKE_SOURCE_DIR@/src \
                         @CMAKE_SOURCE_DIR@/doxygen
RECURSIVE              = YES
FILE_PATTERNS          = *.hpp *.cpp *.md
EXCLUDE_PATTERNS       = */build/* */tests/* */pch.hpp

EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = YES
EXTRACT_STATIC         = YES
JAVADOC_AUTOBRIEF      = YES
QT_AUTOBRIEF           = YES
BUILTIN_STL_SUPPORT    = YES

GENERATE_HTML          = YES
GENERATE_LATEX         = NO
HTML_COLORSTYLE        = DARK

HAVE_DOT               = YES
DOT_IMAGE_FORMAT       = svg
INTERACTIVE_SVG        = YES

WARN_IF_UNDOCUMENTED   = YES
WARN_AS_ERROR          = NO
QUIET                  = YES

PREDEFINED             = FBIDE_DEBUG_BUILD __WXMSW__
```

`EXCLUDE_SYMBOLS = wx*` is a likely follow-up if the inheritance graphs
fill up with wxWidgets internals. Decide after the first run.

### 2.3 Comment style

- `/** … */` — class, struct, enum, free function, non-trivial method.
- `///` — one-liner brief on trivial getters/setters/fields.
- First sentence is the brief (`JAVADOC_AUTOBRIEF`); no explicit `@brief`.

Tags we use:

| Tag                | When                                              |
|--------------------|---------------------------------------------------|
| `@param`           | Only when the name doesn't already convey it.     |
| `@return`          | Only when not obvious from the signature.         |
| `@throws`          | Functions that throw (rare in fbide).             |
| `@note`            | Invariants, non-obvious constraints.              |
| `@warning`         | Lifetime / thread-safety pitfalls.                |
| `@see`, `@ref`     | Cross-link related types and subsystem pages.     |
| `@code{.cpp}`      | Usage examples on entry points (Context, etc.).   |
| `@subpage`         | Only in `mainpage.md` to chain top-level pages.   |

We deliberately skip `@author`, `@date`, `@version` (git tracks those)
and any tag that just restates the type signature.

### 2.4 Manager-class doc block template

Used for every class that lives long-term inside `Context` (or that
manages a non-trivial subsystem). Pin the template once, don't re-state
it in every subpage.

```cpp
/**
 * One-line purpose.
 *
 * Longer paragraph: what it owns, what problem it solves, how it
 * interacts with the rest of the system.
 *
 * **Owns:** ...
 * **Owned by:** Context (lifetime tied to the application).
 * **Threading:** UI thread only / worker thread / mixed (with notes).
 * **Lifecycle:** constructed before X, destroyed after Y.
 */
```

Non-Manager classes get a single descriptive paragraph; sections only
when there's something non-obvious. The template applies to most
Context members but not all — e.g., `InstanceHandler` is conditional
(skipped under `--new-window`); call out deviations in the class block.

### 2.5 clang-format

Single style pass after Phase 5. Driver:

```
"C:\Users\Albert\Developer\clang+llvm-22.1.3-x86_64-pc-windows-msvc\bin\clang-format.exe" \
    -i $(find src tests \( -name "*.hpp" -o -name "*.cpp" \))
```

Use the existing `.clang-format` config in the repo root (verify present
during Phase 0). Do *not* run clang-format mid-phase — let docs land
first, format last.

## 3. Subpage Catalog

Each subpage lives in `doxygen/` and is anchored from `mainpage.md` via
`@subpage`. Subpages cross-link to each other with `@ref`.

| File               | Anchor          | Covers                                                    |
|--------------------|-----------------|-----------------------------------------------------------|
| `mainpage.md`      | `mainpage`      | Top page; lists subsystems + project intro.               |
| `architecture.md`  | `architecture`  | Context-as-locator, ownership graph, app lifecycle.       |
| `commands.md`      | `commands`      | Layout-driven menu/toolbar wiring + dispatch.             |
| `documents.md`     | `documents`     | Document, DocumentManager, save/reload, encoding/EOL.     |
| `analyses.md`      | `analyses`      | Throttled lex+parse pipeline, SymbolTable, recycling.     |
| `editor.md`        | `editor`        | Editor surface: themes, transforms, folding, hotspots.    |
| `compiler.md`      | `compiler`      | Compile/Run/QuickRun lifecycle, error nav, fbc probe.     |
| `format.md`        | `format`        | Lexer → Tree → Renderer formatter pipeline.               |
| `settings.md`      | `settings`      | SettingsDialog tabs + ConfigManager hot-reload chain.     |
| `theming.md`       | `theming`       | Theme schema, .fbt/.ini, ThemeCategory dispatch.          |
| `ui.md`            | `ui`            | UIManager / SideBarManager / freeze locks.                |

Sections roughly follow this skeleton (subpages adapt as needed):

1. **The model** — what the subsystem is, key types in one sentence each.
2. **Lifecycle** — when objects are created, when they go away.
3. **Pipeline / data flow** — ASCII diagram if there's one to draw.
4. **Invariants & threading** — what holds across calls; locking notes.
5. **Recipe** — how a contributor adds the next thing of this kind.
6. **Cross-links** — `@ref` to other subpages where the topic touches.

Outlines below pin the *shape* — the actual prose is written when the
subpage is authored.

### 3.1 `architecture.md`

- Why we use a service-locator (`Context`) rather than DI containers.
- Construction order in `Context::Context` (ConfigManager first; managers
  declared in dependency order; `CommandManager` last so handlers can
  call into anything).
- Reverse destruction; what that buys us (`SideBarManager` before
  UIManager so its non-owning pointers don't dangle).
- Diagram: who holds whom, with arrows for `unique_ptr` ownership and
  raw references for `Context&` callers.
- Threading map: which managers are UI-thread-only, which cross over
  (only `IntellisenseService` does).

### 3.2 `commands.md`

- The model: a *command* is `{name, CommandId enum, optional accelerator,
  set of bindings}` — bindings cover menu items, toolbar tools, AUI
  panes, config-bound checkboxes.
- Resolution chain table:

  | Source                  | Section / key            | Contributes           |
  |-------------------------|--------------------------|-----------------------|
  | `layout.ini`            | `toolbar=`, `[menu]/...` | Existence + position  |
  | `locales/<lang>.ini`    | `[commands/<id>]`        | name / tooltip / help |
  | `shortcuts.ini`         | `<id>=Ctrl+...`          | accelerator           |
  | `config_<plat>.ini`     | `[commands]/<id>=0\|1`   | initial check state   |
  | `CommandId.hpp`         | enum value               | wx event id           |
  | `CommandManager::on<X>` | static event table       | dispatch handler      |

- Worked example: `compile` end-to-end (layout → wire → click → handler).
- State model: `enabled` (broad UI gate from `UIManager::applyState`) vs
  `forceDisabled` (per-editor mask from `DocumentManager::syncEditCommands`).
  Effective state = `enabled && !forceDisabled`.
- Dynamic ranges: recent-files via `wxFileHistory`, external help links
  via `EVT_MENU_RANGE`.
- Recipe: adding a new command (enum / locale / layout / handler).

### 3.3 `documents.md`

- Document = Editor + path + encoding + EOL + modtime + symbol table.
- DocumentManager: open/save/saveAs/close pipelines, single-instance
  forwarding, find/replace dialog, file history integration.
- DocumentIO: encoding detection, BOM handling, EOL normalisation,
  forced-encoding reload.
- FileSession: `.fbs` save/restore (open files, encoding, eol).
- External-change detection on save (`checkExternalChange` + prompt).
- Reload-from-disk path (preserves doc's encoding + EOL).
- Recipe: adding a new document type.

### 3.4 `analyses.md`

- Pipeline diagram (UI thread → throttle → submit → worker → publish):

  ```
  Editor::onModified                              (UI thread)
         │
         ▼   500 ms throttle (m_intellisenseTimer)
  DocumentManager::submitIntellisense
         │
         ▼   wxString → utf8 round-trip (CoW isolation)
  IntellisenseService::submit
         │   m_pending slot (latest-wins) + condvar signal
         ▼                                         ── thread boundary ──
  IntellisenseService::Entry  ↻ worker loop       (worker thread)
         │
         ▼   FBSciLexer::Lex → MemoryDocument
         ▼   StyleLexer::tokenise(m_tokens)        ← token vector reused
         ▼   ReFormatter::buildTree (lean = true)
         ▼   acquireSymbolTable() → populate(tree) ← SymbolTable pool
         │
         ▼   wxQueueEvent(EVT_INTELLISENSE_RESULT)
                                                   ── thread boundary ──
  DocumentManager::onIntellisenseResult            (UI thread)
         │   contains(owner) sanity check
         ▼
  Document::setSymbolTable
         │
         ▼   if active: SymbolBrowser::setSymbols
                          │   skip rebuild when prev/new hash matches
                          ▼
                   leaves indexed via tree-id → Entry map
  ```

- Threading & cancellation: `m_mtx` guards pending+pool; `m_inFlight`
  atomic tag for cancel-safe publish; `cancel(doc)` from `closeFile`
  clears pending and flips the in-flight tag.
- wxString CoW pitfall — why submission round-trips through `utf8_string`.
- Memory recycling: token vector + SymbolTable pool. Steady state =
  active doc count + 1 idle slot (`prune()` from `closeFile`).
- Lexer stack: `FBSciLexer` produces style runs → `StyleLexer` builds
  `Token`s with `KeywordKind`/`OperatorKind`/line annotations.
- Token shape gotcha: a `#include "foo.bi"` line is one merged
  Preprocessor token; identification is `kwKind == PpInclude`; path is
  the first quoted span in the token text.
- SymbolTable: what it captures, hash semantics (kind+name only),
  `findIncludeAt(line)` for Ctrl+click navigation.
- SymbolBrowser dispatch: tree-id → Entry map, hash-based rebuild
  dedup, leaves resolve through the freshest table.
- Recipe: adding a new symbol kind (enum, walk case, locale, icon,
  appendBucket call, dispatchLeaf branch).

### 3.5 `editor.md`

- Editor extends `wxStyledTextCtrl`; one per Document; preview mode
  strips chrome for the Format dialog preview pane.
- Theme & lexer wiring: the `applySettings` chain; per-DocumentType
  theme dispatch (FreeBASIC custom lexer vs Scintilla built-ins for
  HTML / Properties / Text).
- Throttled analysis hook (cross-link to `@ref analyses`).
- On-type pipeline (`CodeTransformer`):
  - `onCharAdded` (single chars).
  - `onTextInserted` (multi-char, deduped against char-added via
    `m_insertHandled`).
  - `onCaretMoved` (selection changes from `onUpdateUI`).
  - `disableTransforms(bool)` during load/reload.
- AutoIndent: block opener / closer / mid-keyword classification;
  single-line `If ... Then ...` detection so it isn't indented.
- Keyword case transform: `editor.transformKeywords` + `editor.keywordCase`.
- Code folding: margin setup + theme-driven marker colors.
- Ctrl+click `#include` navigation: keydown enables hotspot on PP
  styles, hotspot click looks up `findIncludeAt`, calls
  `DocumentManager::openInclude`.
- Find / Replace / GotoLine, Comment / Uncomment, Selection helpers.
- Status bar updates (line:col / EOL / encoding) + brace match +
  edit-command sync, all on the same `UPDATE_UI` event.
- Recipe: adding a new on-type transform.

### 3.6 `compiler.md`

- Players: `CompilerManager` (stable), `BuildTask` (one-shot),
  `CompileCommand` / `RunCommand` (template substitution),
  `AsyncProcess` (wxProcess wrapper).
- Single in-flight task invariant; replacing `m_task` cancels the old.
- Compile flow: validate active doc → ensure saved → build cmd → spawn
  → exit handler parses output → optionally chain into run.
- QuickRun temp file (`BuildTask::TEMPNAME`) lifecycle.
- Command-template variables (`<$fbc>`, `<$file>`, `<$param>`, ...).
- Working directory rule (always source-file dir; old `ActivePath`
  option was removed).
- Error parsing → clickable Output Console rows → `goToError(line, file)`.
- Compiler probe: `resolveCompilerBinary` + `getFbcVersion` cache;
  startup vs build-time prompts; `alerts.ignore.missingCompilerBinary`.
- Compiler log dialog + the `[bold]...[/bold]` markup convention.
- Recipe: adding a new compile-time toggle (config / template / UI).

### 3.7 `format.md`

Lexer + tree-builder are shared with the analysis pipeline; this page
focuses on the rendering side. See @ref analyses for the lexer details.

- Pipeline: `FBSciLexer` (Lexilla styling) → `StyleLexer` (token
  annotation) → `ReFormatter` (segments + dispatch) → `TreeBuilder`
  (`ProgramTree`) → `Renderer` (text out) / `HtmlRenderer` (HTML out) /
  `CaseTransform` (case mode).
- Lean mode: drops layout tokens, collapses blank-line runs. Used by
  the analysis pipeline where tokens drive the SymbolTable directly.
- Verbatim regions (`' format off` / `' format on`): annotation pass +
  pass-through render.
- AnchoredPP rendering: how `#`-at-column-0 mode rebuilds the merged
  PP token text with padding to keep the directive word at the
  expected indent.
- Renderer responsibilities: indentation, inter-token spacing
  (`needsSpaceBefore`), line continuations (`_`), trailing newlines.
- Recipe: adding a new format option.

### 3.8 `settings.md`

- SettingsDialog tabs (`General` / `Theme` / `Keywords` / `Compiler`).
- `Page` enum + `create(initial)` for opening on a specific tab.
- Each panel's `apply()` writes back to `ConfigManager::config()`;
  dialog calls `ConfigManager::save(Config)` on OK.
- ConfigManager categories: `Config`, `Locale`, `Shortcuts`, `Keywords`,
  `Layout` — per-file, all merged into the same logical config tree.
- Hot-reload chain: `reloadIfKnown(path)` triggered when a known
  config file is saved by the user; cascades into `refreshUi` +
  `updateEditorSettigs`.
- `--cfg=<spec>` CLI introspection (cross-link to App docs).
- Recipe: adding a new settings field (model member, panel UI, apply).

### 3.9 `theming.md`

- `Theme` structure: per-category `Entry` (Colors + bold/italic/underline)
  plus top-level fields (font, fontSize, separator, line-number colors,
  selection, fold margin, brace match).
- `ThemeCategory` enum + the `DEFINE_THEME_*` X-macros that keep the
  enum, getter/setter pair, and load/save in sync.
- Loading: `.ini` (canonical) + `.fbt` (legacy v4 read-only migration).
- Save round-trip + `Version` field at the top.
- Per-DocumentType theme application from `Editor::applyTheme`.
- Recipe: adding a new theme entry (X-macro line, theme file update).

### 3.10 `ui.md`

- UIManager: builds the main frame; constructs menu/toolbar/AUI from
  `layout.ini`; owns the `wxAuiNotebook` for documents and the
  status bar.
- `freeze()` pattern (RAII guard against repaint thrash during bulk
  updates).
- SideBarManager: AUI panel notebook for Browser / Subs / etc.;
  `showSymbolsFor(doc)` is the public entry the document pipeline calls.
- `applyState(UIState)` and how it drives broad command enable/disable.
- Status bar fields and who writes which (UIManager owns the bar;
  Editor writes line:col / EOL / encoding; CompilerManager writes the
  status text).

## 4. Documentation Phases

Each phase ends with `cmake --build … --target docs` + fixing warnings
before moving on.

### Phase 0 — Tooling (no prose yet)

- `Doxyfile.in`, `cmake/doxygen.cmake`, hook into root CMakeLists.
- `doxygen/mainpage.md` skeleton with `@subpage` placeholders for §3.
- `.gitignore` add `docs/html/`.
- Verify `.clang-format` exists at repo root.
- Smoke test: `--target docs` builds something.

### Phase 1 — Architecture & subpages

Write all §3 subpages top-down. Markdown only — no header comments
yet. Subpages will reference `@ref` anchors; classes they reference
will get blocks in Phase 2.

### Phase 2 — Manager headers

Walk the dependency chain and document each public/private API
following §2.3 + §2.4.

Order:

1. `Context.hpp`
2. `ConfigManager.hpp`, `Theme.hpp`, `FileHistory.hpp`, `Value.hpp`,
   `Version.hpp`
3. `UIManager.hpp`, `SideBarManager.hpp`, `CommandManager.hpp`,
   `CommandEntry.hpp`, `CommandId.hpp`
4. `DocumentManager.hpp`, `Document.hpp`, `DocumentIO.hpp`,
   `FileSession.hpp`
5. `IntellisenseService.hpp`, `SymbolTable.hpp`, `SymbolBrowser.hpp`
6. `CompilerManager.hpp`, `BuildTask.hpp`, `CompileCommand.hpp`,
   `RunCommand.hpp`, `AsyncProcess.hpp`
7. `HelpManager.hpp`
8. `App.hpp`, `InstanceHandler.hpp`

### Phase 3 — Peripheral classes

- `Editor.hpp`, `CodeTransformer.hpp`, `AutoIndent.hpp`
- `EncodingDetector.hpp`, `TextEncoding.hpp`
- `FBSciLexer.hpp`, `MemoryDocument.hpp`, `StyledSource.hpp`
- `ui/controls/` (`Panel.hpp`, `Layout.hpp`, etc.)
- Settings panels (`GeneralPage.hpp`, `ThemePage.hpp`,
  `KeywordsPage.hpp`, `CompilerPage.hpp`)
- `analyses/lexer/` (`Token.hpp`, `KeywordTables.hpp`,
  `VerbatimAnnotator.hpp`)
- `format/transformers/reformat/` (`FormatTree.hpp`, `ReFormatter.hpp`,
  `TreeBuilder.hpp`, `Renderer.hpp`)
- `format/renderers/` (`HtmlRenderer.hpp`)
- `format/transformers/case/` (`CaseTransform.hpp`)

### Phase 4 — Enums, free helpers, internals

- Standalone enums: `DocumentType`, `EolMode`, `KeywordKind`,
  `OperatorKind`, `ThemeCategory`, `SymbolKind`, `UIState`.
  (`CommandId` lands with `CommandManager` in Phase 2.)
- Anonymous-namespace helpers in `.cpp`s only when the algorithm is
  non-obvious (most won't need it).

### Phase 5 — Polish & format

- Re-run with `WARN_IF_UNDOCUMENTED = YES`; fix gaps.
- Single clang-format pass over `src/` + `tests/`.
- Optional: flip `WARN_AS_ERROR = YES` in CI so the docs target fails
  on undocumented public API.

## 5. Recommendations

1. **Document at the declaration, not the definition.** Doc blocks live
   in `.hpp`; `.cpp` only gets implementation notes when the algorithm
   is non-obvious.
2. **Why over what.** The signature shows what; the doc explains when
   to use it, what it guarantees, and what it costs.
3. **Lifetime + ownership goes on the type, not on every method.**
   Stated once in the class block; methods don't repeat it.
4. **Threading contract goes on the class.** Anything crossing the
   UI/worker boundary gets an explicit "Threading:" section.
5. **Cross-link with `@ref`.** Manager docs link to their subsystem
   page; subsystem pages link back to the key types.
6. **Don't document obvious wxWidgets behavior.** Document our usage
   only where the semantics are non-trivial (e.g., the freeze pattern).
7. **Long-form prose lives in subpages.** Class blocks stay tight.
8. **Embed real code in subpage docs.** `@code{.cpp}` for canonical
   flows reads better than English-only prose.
9. **Single style pass at the end.** clang-format last.
10. **Make the docs target part of the dev loop.**
    `cmake --build build/claude/debug --target docs` after each phase.
11. Rewrite existing code docs for consistency as needed.

## 6. Risks / Open Questions

- **Doxygen + wxWidgets headers.** Inheritance graphs may fill with wx
  classes. Likely fix: `EXCLUDE_SYMBOLS = wx*` after first run.
- **Existing `///` one-liners.** Most are fine and render in Doxygen.
  Promote to `/** */` only when the content grows beyond a single line.
- **WARN_AS_ERROR rollout.** Off during the pass; consider on for CI
  once Phase 5 lands.
