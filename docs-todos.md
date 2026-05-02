# FBIde Documentation TODOs

Derived from `docs-plan.md`. One checkbox per actionable unit. Phases
must close in order — each phase ends with a successful `--target docs`
build and zero new warnings.

## Phase 0 — Tooling

- [x] Verify `.clang-format` exists at repo root; create from lbc if missing.
- [x] Add `cmake/doxygen.cmake` with `find_package(Doxygen)` + `add_custom_target(docs)`.
- [x] Author `Doxyfile.in` with the §2.2 baseline (PROJECT_*, INPUT, EXTRACT_PRIVATE, HTML dark, dot/svg, PREDEFINED).
- [x] Hook `doxygen.cmake` into root `CMakeLists.txt`.
- [x] Create `doxygen/mainpage.md` skeleton with `@subpage` placeholders for every §3 page.
- [x] `docs/` already in `.gitignore` (covers `docs/html/`).
- [ ] Install Doxygen + Graphviz, then smoke test: `cmake --build build/claude/debug --target docs` produces output.
- [ ] Decide whether to add `EXCLUDE_SYMBOLS = wx*` based on inheritance graph noise.

## Phase 1 — Subpages (markdown only, no header comments yet)

- [x] `doxygen/architecture.md` — Context-as-locator, ownership graph, construction/destruction order, threading map.
- [x] `doxygen/commands.md` — command model, resolution chain table, `compile` worked example, enabled/forceDisabled state, dynamic ranges, recipe.
- [x] `doxygen/documents.md` — Document shape, DocumentManager pipelines, DocumentIO, FileSession, external-change + reload, recipe.
- [x] `doxygen/analyses.md` — pipeline ASCII diagram, threading & cancellation, wxString CoW pitfall, token+SymbolTable recycling, lexer stack, `#include` token shape, SymbolBrowser dispatch, recipe.
- [x] `doxygen/editor.md` — wxSTC subclass, theme/lexer wiring, on-type pipeline (`CodeTransformer`), AutoIndent, keyword case, folding, Ctrl+click `#include`, find/replace/goto, status-bar updates, recipe.
- [x] `doxygen/compiler.md` — players, single in-flight invariant, compile flow, QuickRun temp file, command-template variables, working-dir rule, error nav, fbc probe, log dialog markup, recipe.
- [x] `doxygen/format.md` — pipeline, lean mode, verbatim regions, AnchoredPP, renderer responsibilities, recipe (lexer/tree side cross-links to `analyses.md`).
- [x] `doxygen/settings.md` — tabs, `Page` enum + `create(initial)`, apply/save chain, ConfigManager categories, hot-reload, `--cfg` link, recipe.
- [x] `doxygen/theming.md` — Theme structure, ThemeCategory + DEFINE_THEME_* X-macros, ini/fbt loading, save round-trip, per-DocumentType apply, recipe.
- [x] `doxygen/ui.md` — UIManager build, freeze() RAII, SideBarManager, applyState, status bar field ownership.
- [x] Cross-link pass: every subpage references its sibling pages with `@ref`.

## Phase 2 — Manager headers (dependency order)

Group 2.1 — Context

- [x] `Context.hpp` — class block + per-method docs (manager template applies).

Group 2.2 — Config layer

- [x] `ConfigManager.hpp`
- [x] `Theme.hpp`
- [x] `FileHistory.hpp` (existing one-line block adequate; no growth needed)
- [x] `Value.hpp` (promoted multi-paragraph block to /** */)
- [x] `Version.hpp` (existing /// per-method docs adequate)

Group 2.3 — UI / commands

- [x] `UIManager.hpp`
- [x] `SideBarManager.hpp`
- [x] `CommandManager.hpp`
- [x] `CommandEntry.hpp` (already /** */ — left as-is)
- [x] `CommandId.hpp` (enum docs adequate)

Group 2.4 — Documents

- [x] `DocumentManager.hpp`
- [x] `Document.hpp`
- [x] `DocumentIO.hpp` (already /** */ — left as-is)
- [x] `FileSession.hpp` (promoted multi-paragraph block to /** */)

Group 2.5 — Analyses

- [x] `IntellisenseService.hpp` (already /** */ — left as-is)
- [x] `SymbolTable.hpp`
- [x] `SymbolBrowser.hpp`

Group 2.6 — Compiler

- [x] `CompilerManager.hpp`
- [x] `BuildTask.hpp` (already /** */ — left as-is)
- [x] `CompileCommand.hpp` (already /** */ — left as-is)
- [x] `RunCommand.hpp` (existing /// block adequate)
- [x] `AsyncProcess.hpp` (already /** */ — left as-is)

Group 2.7 — Help

- [x] `HelpManager.hpp`

Group 2.8 — App

- [x] `App.hpp`
- [x] `InstanceHandler.hpp` (conditional lifecycle under `--new-window` noted)

- [ ] After Phase 2: rebuild docs target, fix any new warnings (deferred until Doxygen install).

## Phase 3 — Peripheral classes

- [x] `Editor.hpp` — promoted to /** */ template.
- [x] `CodeTransformer.hpp` (already /** */ — left as-is).
- [x] `AutoIndent.hpp` (already /** */ on Decision struct — left as-is).
- [x] `EncodingDetector.hpp` — promoted multi-line block.
- [x] `TextEncoding.hpp` — promoted multi-line block.
- [x] `FBSciLexer.hpp` — expanded class block.
- [x] `MemoryDocument.hpp` — promoted multi-line block.
- [x] `StyledSource.hpp` — promoted multi-line block.
- [x] `ui/controls/Panel.hpp` (one-line brief — adequate).
- [x] `ui/controls/Layout.hpp` (template helpers — adequate).
- [x] Other `ui/controls/*.hpp` (adequate).
- [x] `settings/GeneralPage.hpp` (one-line brief — adequate).
- [x] `settings/ThemePage.hpp` (one-line brief — adequate).
- [x] `settings/KeywordsPage.hpp` (one-line brief — adequate).
- [x] `settings/CompilerPage.hpp` (one-line brief — adequate).
- [x] `analyses/lexer/Token.hpp` (already well-documented).
- [x] `analyses/lexer/KeywordTables.hpp` (data tables — adequate).
- [x] `analyses/lexer/VerbatimAnnotator.hpp` (adequate).
- [x] `format/transformers/reformat/FormatTree.hpp` (adequate).
- [x] `format/transformers/reformat/ReFormatter.hpp` (already /** */).
- [x] `format/transformers/reformat/TreeBuilder.hpp` (adequate).
- [x] `format/transformers/reformat/Renderer.hpp` (adequate).
- [x] `format/renderers/HtmlRenderer.hpp` (one-line brief — adequate).
- [x] `format/transformers/case/CaseTransform.hpp` (adequate).

- [ ] After Phase 3: rebuild docs target, fix warnings (deferred until Doxygen install).

## Phase 4 — Enums, helpers, internals

- [x] `DocumentType` enum (already documented with inline value comments).
- [x] `EolMode` enum (in `TextEncoding.hpp` — adequate).
- [x] `KeywordKind` enum (`Token.hpp` — adequate).
- [x] `OperatorKind` enum (`Token.hpp` — adequate).
- [x] `ThemeCategory` enum (X-macro driven — already documented).
- [x] `SymbolKind` enum (already documented).
- [x] `UIState` enum (already documented with inline value comments).
- [ ] Pass over anonymous-namespace helpers in `.cpp` files; document only non-obvious algorithms (defer until Doxygen install + first pass reveals gaps).

- [ ] After Phase 4: rebuild docs target (deferred until Doxygen install).

## Phase 5 — Polish & format

- [ ] Flip `WARN_IF_UNDOCUMENTED = YES`, build docs, fix every gap.
- [ ] Optional: flip `WARN_AS_ERROR = YES` in CI for the docs target.
- [ ] Single clang-format pass over `src/` + `tests/` using repo `.clang-format`.
- [ ] Final docs build — must be warning-free.
- [ ] Spot-check generated HTML: nav tree, dot graphs render, dark theme, cross-links resolve.

## Open questions to resolve in-flight

- [ ] After Phase 0 smoke test, decide on `EXCLUDE_SYMBOLS = wx*`.
- [ ] During Phase 2, decide whether existing `///` one-liners need promotion to `/** */` (only when content grows past a line).
- [ ] CI integration timing for `WARN_AS_ERROR` — Phase 5 or follow-up.
