# Analyses {#analyses}

The analyses subsystem turns editor text into a per-document
`SymbolTable` for the sidebar's Sub/Function browser, the Ctrl+click
`#include` jump, and any other tooling that wants a structural view of
the buffer. It is the only piece of FBIde that runs on a worker
thread, and the trickiest threading and lifetime work in the codebase
lives here.

## The model

| Type                                | Role                                                      |
|-------------------------------------|-----------------------------------------------------------|
| `IntellisenseService`               | Owns the worker. Single-task slot, latest-wins.           |
| `FBSciLexer` (Lexilla wrapper)      | Produces style runs over a `MemoryDocument`.              |
| `StyleLexer`                        | Folds runs into `lexer::Token`s with keyword + op kinds.  |
| `ReFormatter` (lean mode) + `TreeBuilder` | Tokens to `reformat::ProgramTree`.                  |
| `SymbolTable`                       | Walked from the tree; flat vectors per `SymbolKind`.      |
| `SymbolBrowser` (sidebar)           | Renders the table; tree-id to `Entry` lookup map.         |

`IntellisenseService` is owned by `DocumentManager` and declared *last*
in the field list so destruction joins the worker before the documents
and transformer it might race with go away.

## Pipeline

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

## Threading & cancellation

`m_mtx` guards the pending slot, the SymbolTable pool, and the
shutdown flag. The worker waits on `m_cv` for either a new task or
`m_stopRequested`. There is no queue — `submit` overwrites whatever
was pending; the worker only ever sees the most recent snapshot.

`m_inFlight` is an `std::atomic<const Document*>`, set to the task tag
just before processing and cleared on completion. `cancel(doc)`
clears any pending task tagged with `doc` and CAS-clears `m_inFlight`
to `nullptr` if it currently matches — so an in-progress parse for a
closing document drops its result rather than publishing it.

A second guard runs on the UI side: `onIntellisenseResult` calls
`DocumentManager::contains(owner)` before applying. The `Document*`
is only ever a *tag* — the worker never dereferences it; the handler
treats it as opaque until `contains` confirms it still exists.

## wxString CoW pitfall

`wxString` is copy-on-write. Naïvely passing it to a worker thread
shares the underlying refcounted buffer; if the UI thread mutates the
shared instance, the worker sees torn data. `submit` round-trips the
content through `std::string` (utf-8 encoded) at the thread boundary
to deep-copy the payload. The worker's `MemoryDocument` borrows that
string for the duration of the parse.

## Memory recycling

Two reusable pools keep allocations off the hot path:

- **Token vector** — `m_tokens` is owned by the worker and reused
  every parse. `StyleLexer::tokenise(m_tokens)` clears + appends.
- **SymbolTable pool** — `m_pool` is a `vector<shared_ptr<SymbolTable>>`
  guarded by `m_mtx`. `acquireSymbolTable` finds the first slot with
  `use_count == 1` (only the pool holds it) and reuses it; otherwise it
  allocates and grows the pool. `populate` on a pooled instance keeps
  the inner vectors' capacity. `prune()` is called from
  `closeFile` to evict idle slots beyond the steady-state target —
  active doc count + 1 idle slot.

A slot's `use_count` doubles as a liveness flag: 1 means idle (only
the pool); >1 means a `Document` or in-flight event payload still
references it.

## Lexer stack

`FBSciLexer` reuses Scintilla's Lexilla machinery to colorize a
`MemoryDocument`. The styled output is a flat run of `(start, length,
style)` tuples. `StyleLexer::tokenise` runs over those runs and
emits `lexer::Token` records with `KeywordKind` and `OperatorKind`
annotations and per-line tagging. The token kind set lives in
`Token.hpp` and mirrors the theme keyword groups (so the same
classifier feeds both syntax highlighting and analyses).

### `#include` token shape gotcha

A line like `#include "foo.bi"` arrives as **one** merged
`Preprocessor` token covering the whole line. Identification is
`kwKind == PpInclude`; the path is the first quoted span inside the
token text. Don't expect a separate string token — there isn't one.

## SymbolTable

`SymbolTable` (`src/lib/analyses/symbols/SymbolTable.hpp`) holds flat
vectors per `SymbolKind`: Subs, Functions, Types, Unions, Enums,
Macros, plus a separate `Includes` vector. The walk:

- Visits top-level Sub / Function / Type / Union / Enum.
- Captures `#macro NAME` definitions; the macro name is parsed out
  of the merged Preprocessor token text (just like `#include`).
- Recurses into Namespace bodies (flat list — no qualified names
  yet).
- Skips anonymous declarations.
- Intentionally ignores Constructor / Destructor / Operator at this
  stage.

`m_hash` is a stable hash over `(kind, name)` pairs only — line
numbers don't participate. Consumers use it to skip rebuilds when
nothing meaningful changed (e.g., the user added blank lines).

`findIncludeAt(line)` powers Ctrl+click navigation: the editor's
hotspot click lands here to resolve the line into an `Include` record,
which `DocumentManager::openInclude` then opens.

## SymbolBrowser dispatch

`SymbolBrowser` (`src/lib/sidebar/SymbolBrowser.hpp`) is a
`wxTreeCtrl` subclass living in the sidebar notebook. The static
event table dispatches directly without any `PushEventHandler` —
avoids teardown-order races during frame close.

`setSymbols(doc)` is the single public entry point. Three short-circuit
cases avoid rebuilding the tree:

1. The new shared_ptr is the same as the current one.
2. The new table's hash matches the previous one.
3. The doc is null and the tree is already empty.

When a rebuild is needed, `appendBucket` walks each `SymbolKind`
vector and registers an `Entry { kind, index }` for every leaf, keyed
by the leaf's `wxTreeItemId::Type` (the underlying `void*`).
`onItemActivated` looks up the entry and `dispatch` resolves the
action: navigate the active editor for symbol leaves, or call
`DocumentManager::openInclude` for `#include` leaves.

The `Entry` map is rebuilt every time, so leaves always resolve
through the freshest table.

## Recipe: add a new symbol kind

1. Add a value to `enum class SymbolKind` (`SymbolTable.hpp`).
2. Add a `vector<Symbol>` field to `SymbolTable` and a getter.
3. Teach `SymbolTable::walkBlock` (or `walkNodes`) to recognise the
   construct and `emit` the new kind.
4. Add a locale string for the bucket folder name (display label).
5. Add a tree icon if the new kind warrants it.
6. Call `appendBucket` for the new bucket in
   `SymbolBrowser::rebuild`.
7. Branch on the new kind in `SymbolBrowser::dispatch` if its
   activation differs from "go to line".

## Cross-links

- @ref documents — `submitIntellisense` / `cancelIntellisense`
  contract and `Document::setSymbolTable`.
- @ref editor — modify-throttle timer that drives submission and the
  Ctrl+click hotspot path.
- @ref format — `ReFormatter` + `TreeBuilder` shared with the
  formatter; lean mode is what analyses uses.
- @ref ui — `SideBarManager::showSymbolsFor` is the public entry the
  document pipeline calls.
