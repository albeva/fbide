# Editor {#editor}

The editor surface — the actual text-editing widget the user types
into — is `fbide::Editor`, a `wxStyledTextCtrl` (Scintilla) subclass.
There is exactly one `Editor` per `Document`, plus one preview-mode
instance per Format dialog.

## The model

```
DocumentManager
   │ owns Documents
   ▼
Document
   │ owns one Unowned<Editor>
   ▼
Editor : wxStyledTextCtrl
   │ uses
   ▼
CodeTransformer (shared, owned by DocumentManager)
   │ uses
   ▼
indent::decide       (static, in AutoIndent.hpp)
```

`Editor`'s own state is small: a `CodeTransformer*` (nullable for
preview mode), the `DocumentType`, current font, and a few latches
that coordinate the `wxSTC` event firehose. The heavy lifting
(transforms, theme dispatch, lexer config) lives in collaborators.

### Preview mode

Construction takes a `preview` flag. When set, `Editor` strips margins
and decorations and is wired without a transformer. The Format dialog
uses one preview editor for the "before" pane and another for the
"after" pane to render `Renderer` output without polluting the user's
buffer.

## Theme & lexer wiring

`Editor::applySettings` is the entry point for "the config changed,
rewire me". It calls into a per-`DocumentType` dispatch — FreeBASIC
gets the custom `FBSciLexer`, while HTML / Properties / Text route
to Scintilla's built-in lexers via `applyHtmlTheme` /
`applyPropertiesTheme` / `applyTextTheme`. Theme entries are pulled
from `ConfigManager`'s `Theme` and translated to `wxSTC` style ids.
See @ref theming for the schema.

## Throttled analysis hook

Modifying the buffer restarts `m_intellisenseTimer` (500 ms). When
it fires, the editor submits the current text to
`DocumentManager::submitIntellisense`, which forwards to the worker.
See @ref analyses for the rest of the pipeline.

## On-type pipeline

`CodeTransformer` (`src/lib/editor/CodeTransformer.hpp`) is the shared
on-type driver: auto-indent on Enter, keyword case normalisation, and
auto-insertion of matching closing keywords. Single instance reused
across all editors — only the active editor drives it at any given
moment, so a shared internal buffer is safe.

### Event surface

| Event                  | Calls                                                         |
|------------------------|---------------------------------------------------------------|
| `EVT_STC_CHARADDED`    | `onCharAdded` — single-char path. Sets `m_insertHandled`.     |
| `EVT_STC_MODIFIED`     | `onTextInserted` — multi-char inserts (paste, drop). Skipped if `m_insertHandled` was just set. |
| `EVT_STC_UPDATEUI`     | `onCaretMoved` — selection / caret position changes.          |

The `m_insertHandled` flag deduplicates the single/multi-char paths:
when a single keystroke fires `onCharAdded` first, the follow-up
`MODIFIED` event for the same insertion is short-circuited so the
caret-moved path doesn't double-transform.

### Style-byte short-circuit

`CodeTransformer` reads the style bytes that `FBSciLexer` already
published into the editor's document — it does not run its own lex
of the editor content. Field-access suppression (words after `.` or
`->`, including across `_` line continuation) and asm / comment /
string contexts come for free from the existing styling.

`disableTransforms(bool)` from `Editor` toggles the transformer off
during file load / reload to avoid touching the buffer while it's
being filled.

## AutoIndent

`indent::decide` (`src/lib/editor/AutoIndent.hpp`) is pure
classification: given the previous line's tokens, return a
`Decision` carrying:

- `deltaLevels` — indent delta for the new line (`+1` opens a block,
  `0` continues, `-1` reserved).
- `dedentPrev` — set when the previous line is itself a closer or
  mid-block keyword (`End If`, `Loop`, `Else`, `Case`).
- `closerKeywords` — span into a static array of canonical closing
  words (`{"loop"}`, `{"end","sub"}`). Empty = no auto-closer.

Single-line `If ... Then ...` is detected here so it isn't indented.
The Editor-facing overload tokenises via `WxStcStyledSource` +
`StyleLexer`; the token-only overload is what unit tests target.

## Keyword case transform

`CodeTransformer::applyWordCase` rewrites the just-finished word to
the configured `CaseMode` for its keyword group (per
`editor.keywordCase[<group>]`). `editor.transformKeywords` is the
master switch. The transform also runs over freshly pasted ranges via
`onTextInserted`.

## Code folding

`Editor::defineFoldMargins` configures the fold margin and registers
marker colors driven by the active theme. `onMarginClick` toggles
folds. The fold structure itself comes from `FBSciLexer`'s SCI fold
levels — the editor only handles the visual margin and click
dispatch.

## Ctrl+click `#include` navigation

- `onKeyDown` / `onKeyUp` toggle hotspots on Preprocessor styles when
  Ctrl is held.
- `onHotSpotClick` resolves the clicked line via
  `Document::getSymbolTable()->findIncludeAt(line)`.
- The found `Include` is forwarded to
  `DocumentManager::openInclude(*doc, include.path)`, which resolves
  against the source dir, then `fbc/inc`, then cwd, and opens the
  file (creating a tab if needed).

`setIncludeHotspots(bool)` flips Scintilla's hotspot style on/off
on Preprocessor styles so the modifier-key visual matches the active
state.

## Find / Replace / GotoLine

The editor exposes the primitives — `findNext`, `replaceNext`,
`replaceAll`, `gotoLine`, `selectLine`, `commentSelection` /
`uncommentSelection` — and `DocumentManager` owns the dialog state
(`m_findData`) and routes find/replace events back through these
methods.

## Status bar updates

`onUpdateUI` is the single coalesced "something changed" event. From
it we run:

- `updateStatusBar()` — line:col + EOL + encoding fields.
- `updateBraceMatch()` — Scintilla brace match.
- `m_ctx.getDocumentManager().syncEditCommands()` — refresh
  Undo/Redo/Cut/Copy/Paste `forceDisabled` masks.

Coalescing onto a single event keeps the per-keystroke cost low.

## Recipe: add a new on-type transform

1. Add the trigger condition to `CodeTransformer::onCharAdded` (or
   `onTextInserted` for multi-char paths).
2. Read the style byte at the trigger position to inherit
   asm / comment / string suppression.
3. Mutate the buffer via `Editor`'s wxSTC API.
4. If the transform is configurable, add a setting key under
   `editor.*` and wire its read into `applySettings`.

## Cross-links

- @ref documents — `Document` lifetime and the `Editor` it owns.
- @ref analyses — modify-throttle that drives `submitIntellisense`
  and the symbol table that backs Ctrl+click.
- @ref theming — per-DocumentType theme dispatch.
- @ref commands — edit menu wiring + `syncEditCommands` mask.
