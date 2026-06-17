# Intellisense — "next level" rewrite

Status: **ideas / design notes** (not committed work). A roadmap for evolving the
current heuristic intellisense into a proper, editor-first language frontend for
FreeBASIC.

## Where we are today

The current pipeline piggybacks on `FBSciLexer` (Scintilla styling) via a
`StyleLexer` adapter to produce tokens, scans them into a lean `ProgramTree`
(`analyses/parser`), and derives a heuristic `SymbolTable`. It powers the symbol
browser, scope/keyword matching, and statement-start code completion (symbols,
type members/fields, in-scope locals/params, keyword groups).

It works, but it's structurally limited: no real parse tree, no semantic
analysis, no macro expansion, no cross-file/include awareness, declaration
parsing is heuristic, and lexing is a second-hand product of the display lexer.

## Guiding principle: no big-bang rewrite

The single biggest determinant of success is **building the new engine in
parallel behind the current one and cutting over at one clean boundary**, so the
editor's features keep working the entire way.

Reusable scaffolding we already have:

- the async worker (latest-wins, pooling, hash-gated publish);
- the editor integration points (timer, indicators, browser, completion popup);
- the `analyses/parser` layer split.

Two scoping reliefs up front:

- **`FBSciLexer` stays** for *display* coloring. A semantic lexer vs a display
  lexer is a normal split — don't try to unify them.
- **The formatter doesn't need any of this.** It only wants structure for
  indent; leave it on its lean parser. The rewrite is scoped to the *analysis*
  pipeline alone, which meaningfully shrinks the blast radius.

## Foundations to nail first (expensive to change later)

- **Source-location model** — the keystone. A SourceManager mapping every token
  to its *spelling* vs *expansion* location across macros + includes. Land this
  *before* macros, or retrofitting is painful; without it, goto-def / rename /
  diagnostics point into the wrong place inside macro/included code.
- **Token model + trivia** — lossless (whitespace/comments attached) so rename /
  refactor can rewrite source exactly; byte offsets, not line/col;
  arena-allocated.
- **AST: error-tolerant + lossless** — must parse *broken* code (the editor's
  steady state) into a usable tree with missing/skipped markers. A red/green
  design (immutable shareable green nodes + lazily-positioned red nodes) buys
  structural sharing, cheap incremental reparse, and a thread-safe immutable
  snapshot to hand the UI.
- **Memory** — per-document arena + workspace-wide identifier **interning**
  (compare by id, dedup). Worker holds the in-flight arena, UI holds last-good,
  old frees when unreferenced.
- **Service / query API** — shape it like LSP even in-process: `completion@pos`,
  `definition@pos`, `references`, `diagnostics`, `signatureHelp`.
  **Query-on-demand** (compute only what a feature asks at a position) beats
  eager whole-program analysis for responsiveness and pairs with a cancellation
  model (we already have latest-wins).

## Hard subsystems + how to scope them

- **Preprocessor (macros + includes)** is the big one, and where the source-maps
  pay off. Best-effort expansion that tolerates incomplete code. Includes feed a
  **module cache** keyed by `path + mtime + (defines/options)` so `windows.bi` /
  `fbgfx.bi` aren't re-parsed per keystroke — that's the real perf unlock.
- **Semantics: pick a "good enough" subset.** Full FB type/overload checking ≈
  reimplementing fbc's frontend (person-years). The high-value 20%: scopes +
  declaration hoisting + name binding + *declared* types → unlocks true member
  completion (`.` / `->`), goto-def/decl, rename. Defer overload resolution,
  constant folding, full type inference.
- **Project context** — includes + compiler options (`-i`, `-d`) define the
  compilation unit; wire fbide's existing compiler configs in as the analysis
  context.
- **Cross-file incrementality** — file → include dependency graph; re-analyze
  dependents when an include changes.
- **Testing** — golden / snapshot tests at each layer (lex, parse, expand, bind)
  are non-negotiable for a frontend; bake them in from phase 1.

## Reference architectures worth stealing from

- **rust-analyzer** — closest blueprint: editor-first, lossless trees (rowan),
  salsa query-based lazy/incremental semantics.
- **Roslyn** — red/green tree specifics.
- **Clang `SourceManager`** — the macro/include location model.

FB has no compiler-as-library and no existing language server, so it's all
bespoke — a big lift, but that's also why the payoff is high (nothing else
exists).

## Staged roadmap (each phase ships value; editor never breaks)

0. **Design doc** — location model, token/AST taxonomy, memory model, service
   API.
1. **Lexer** — standalone + golden tests, run alongside the current pipeline.
2. **Error-tolerant parser → AST** — keep emitting today's `SymbolTable` *from
   the AST* → **cut-over point** (existing features now ride the new tree).
3. **Preprocessor** — macros + includes + source maps + module cache.
4. **Semantic layer** — scopes / binding / declared types → member completion,
   goto-def.
5. **Services** — references / rename, diagnostics, signature help.

## Open strategic questions

- **In-process vs LSP server.** Lean: in-process library with an LSP-shaped API;
  expose as a real LSP server later if VS Code reuse is wanted — don't pay the
  protocol/serialization cost now.
- **Incremental vs full reparse.** Lean: start with full-reparse-on-debounce
  (FB files are mostly small); add incremental reparse only where profiling says
  it hurts.

## Scope reminder

This is weeks-to-months of phased work. The location / AST / memory / API
decisions in *Foundations* are the costly-to-revisit ones; everything else keys
off them, so they come first.
