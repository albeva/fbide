# Auto-indent Plan

Port fbide-old's Enter-based auto-indent feature, plus optional auto-close
of matching block keywords.

## Goal review

**In scope:**

1. Enter after block opener → new line indented +1.
2. Typing block closer (`End If`, `Loop`, `Next`, `Wend`, `End Sub`, ...)
   → that line dedented to match opener.
3. Mid-block keywords (`Else`, `ElseIf`, `Case`) → dedent that line,
   re-indent next.
4. Bonus: opener + Enter → auto-insert matching closer below; cursor
   lands on indented blank line between.

**Skip cases (must NOT indent / open a block):**

- `If x Then stmt` — single-line form, `Then` not last significant keyword.
- `If x Then : stmt :` — colon separator.
- `Declare Sub / Declare Function` — no body.
- `Exit Sub`, `Continue For`, etc. — early exit.
- `Type X As Integer` — alias form.
- Line ends in `_` continuation — treat joined physical lines as one
  logical statement.

**Existing infrastructure to reuse:**

- `fbide::lexer::Lexer` with `KeywordKind` + recent structural fallback in
  `identifier()` (catches `endif` without keyword groups loaded).
  → single source of truth for FB syntax classification.
- `ReFormatter::dispatch()` (ReFormatter.cpp:210–296) already encodes
  block-open/close rules, `firstKeyword()`, `lastSignificantKeyword()`,
  `isBodyDefinition()`, `hasBlockCloserAfterFirst()`. Port these helpers
  into the indent module.
- `Editor` already wires tab config: `tabSize`, `useTabs`, `indent`,
  `tabIndents`, `backspaceUnindents`.
- `EVT_STC_CHARADDED` slot on `Editor` is unused — ready to bind.

**Out of scope (defer):**

- Smart re-indent on paste.
- Bracket-matching auto-indent.
- Full-buffer re-format (Format command covers this).
- Tracking manual closer insertion elsewhere in buffer.

## Architecture

**Pure-function core.** `indent::decide(prevLine, currentLine, options)`
takes strings, returns a `Decision`. No wx, no Editor, no Context. Fully
unit-tested.

**Thin editor glue.** `EVT_STC_CHARADDED` on `\n` reads prev line,
calls `decide()`, applies via `SetLineIndentation()` + optional
`InsertText()` inside `BeginUndoAction` / `EndUndoAction`.

**Two phases.** PR1 = auto-indent only. PR2 = auto-close. Both
feature-flagged in config.

**Reuse Lexer, no new scanner.** `Lexer { {} }` (empty keyword groups)
classifies structural keywords via our fallback path. Cost: one
tokenisation per Enter on the relevant line(s) — negligible.

**Closer map.** Hardcoded, ~15 entries, doesn't belong in config.

## File layout

- `src/lib/editor/AutoIndent.hpp` — public API (`Decision`, `decide()`, `closerFor()`).
- `src/lib/editor/AutoIndent.cpp` — impl.
- `tests/AutoIndentTests.cpp` — `decide()` coverage.
- `src/lib/editor/Editor.cpp` — bind event, apply decision.
- `resources/ide/config_*.ini` — defaults for `editor.autoIndent` / `editor.autoClose`.

## TODOs

### PR1 — Auto-indent (Enter-based)

1. Create `AutoIndent.hpp`:
   ```cpp
   struct Decision {
       int  deltaLevels;   // indent delta for new line
       bool dedentPrev;    // closer on prev line → dedent prev line
   };
   auto decide(const wxString& prevLine,
               const wxString& currentLine,
               int currentIndentLevel) -> Decision;
   ```
2. Create `AutoIndent.cpp` — tokenise with `Lexer { {} }`. Port
   `firstKeyword` / `lastSignificantKeyword` / colon-after-first /
   continuation-stitch / `isBodyDefinition` from ReFormatter.
3. `tests/AutoIndentTests.cpp` — cover:
   - Openers: If+Then, Do, For, While, Sub / Function / Constructor /
     Destructor / Operator, Select, Type (not As), Enum, Union, With,
     Namespace, Scope, Asm.
   - Closers: End If, End Sub, End Select, Loop, Next, Wend, `EndIf`.
   - Mid: Else, ElseIf, Case.
   - Skips: `If x Then stmt`, `If x Then : stmt :`, `Declare Sub`,
     `Exit Sub`, `Type X As Integer`, `Function = 10`.
   - Continuations: `If x _` + next line `Then`.
   - Trailing comments: `If x Then ' comment` still opens.
4. Bind `EVT_STC_CHARADDED` in `Editor.cpp` event table.
5. Implement `Editor::onCharAdded` — on `\n` gather prev line (joining
   `_` continuations), call `decide()`, apply `SetLineIndentation()`.
6. Add `editor.autoIndent` config key, read in Editor settings load,
   gate handler on it.
7. Default `autoIndent=true` in `resources/ide/config_win32.ini` /
   `config_linux.ini`.
8. Add CMake entries for new sources + header.
9. Manual test: launch IDE, type `If x Then` + Enter, verify indent.
10. Commit.

### PR2 — Auto-close (opener → insert closer)

11. Extend `Decision` with `std::optional<wxString> insertCloser`.
12. Add `closerFor(KeywordKind, segmentContext) -> wxString` map —
    returns `End If` / `Loop` / `Next` / `End Sub` / etc.
13. Update `decide()` to populate `insertCloser` when autoClose
    enabled AND opener detected.
14. Editor handler: inside `BeginUndoAction` / `EndUndoAction` insert
    `\n<indent><closer>`, then move caret to indented blank line above.
15. Extend tests — assert `insertCloser` populated correctly per opener.
16. Add `editor.autoClose` config key, default false (opinionated).
17. GeneralPage settings UI toggle for both flags.
18. Manual test: verify undo collapses the auto-close edit into one step.
19. Commit.

## Risks / decisions

- **`For i = 1 To 10` closer**: `Next` or `Next i`? → default `Next`
  (matches FB reference, avoids guessing var name on multi-var `For`).
- **Tabs vs spaces on inserted closer**: respect `editor.useTabs`.
- **Cursor position after auto-close**: `GotoLine` + `LineEnd` on the
  indented blank line.
- **Mid-block `Else` / `ElseIf` dedent timing**: dedent-on-type vs
  dedent-on-Enter? → v1 does dedent-on-Enter (simpler, matches old
  fbide). Revisit if it feels clunky.
