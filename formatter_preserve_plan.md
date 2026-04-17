# Formatter Preservation Options — Implementation Plan

Add two new `FormatOptions` flags that let the formatter partially preserve the
original source layout:

- **`reIndent`** (default `true`) — when `false`, each statement's leading
  whitespace tokens are emitted verbatim instead of `emitIndent(depth)`.
- **`reFormat`** (default `true`) — when `false`, inter-token whitespace
  (incl. continuation newlines and colon-separated sequences) is echoed
  verbatim instead of being rebuilt by `needsSpaceBefore`.

Four meaningful combinations:

| reIndent | reFormat | Behavior                                                |
|----------|----------|---------------------------------------------------------|
| true     | true     | Current behavior: full reformat + reindent              |
| false    | true     | Preserve original indent; normalize inter-token spacing |
| true     | false    | Rebuild indent; preserve original inter-token layout    |
| false    | false    | Near-passthrough (identity on well-formed input)        |

---

## Design

### Scanner changes

Stop discarding whitespace/newline tokens. Push them into `m_segment`:

- Leading whitespace of a physical line becomes the first tokens of the statement.
- Continuation: keep the `Newline` after `_` inside the segment.
- Colon-split: only when `reFormat=true`. Under `reFormat=false`, colons stay
  as ordinary tokens in the segment so their surrounding whitespace survives
  untouched.
- Dispatch helpers (`firstKeyword`, `lastSignificantKeyword`,
  `isBodyDefinition`) skip Whitespace/Newline the same way they already skip
  comments; block detection unchanged.
- Blank-line collapse is gated by `reFormat=true`.

### Renderer changes

Branch per statement:

- Leading run of Whitespace tokens → consumed once. Emitted as original text
  (`reIndent=false`) or replaced with `emitIndent(indent)` (`reIndent=true`).
- Remaining tokens:
  - `reFormat=true` → current skip-ws + `needsSpaceBefore` logic.
  - `reFormat=false` → emit every token's text verbatim; suppress the auto
    trailing `\n` if the statement already ends in a Newline.
- Auto "blank line between definitions" suppressed when `reFormat=false`.
- `anchoredPP` remains meaningful only with `reIndent=true`; with
  `reIndent=false` we emit the original leading whitespace and bypass the
  anchor logic for that line.

---

## Incremental Todos

Steps 1–6 are neutral refactors (all existing tests must keep passing).
Steps 7–11 introduce the new behaviors one flag-interaction at a time.

- [x] **1. Extend `FormatOptions`** in `FormatTree.hpp` with `reIndent`/
      `reFormat` bools (both default `true`). No behavior change.

- [x] **2. Thread options into Scanner**: change `Scanner::scan(tokens)` →
      `Scanner::scan(tokens, options)`; update `Formatter::format()`. Scanner
      stores options, still unused. All tests green.

- [x] **3. Renderer forward-compat**: in the current `reFormat=true` render
      path, skip Whitespace/Newline tokens defensively when iterating
      `stmt.tokens` (both for `needsSpaceBefore` and emission). No Scanner
      change yet; all 238 tests remain green.

- [x] **4. Scanner dispatch helpers**: update `firstKeyword`,
      `lastSignificantKeyword`, `isBodyDefinition` to skip Whitespace/Newline
      alongside comments. All tests green.

- [x] **5. Scanner keeps leading whitespace**: restructure `run()` so
      whitespace-only lines are still detected as blanks, but when a real
      statement follows, its leading whitespace tokens flow into `m_segment`
      via `processLine()`. Tests remain green because Renderer now filters
      them (step 3).

- [x] **6. Scanner keeps continuation newline**: in `processLine()`, push the
      `Newline` after `_` into `m_segment` instead of silently advancing past
      it. Tests remain green (Renderer filters).

- [x] **7. Implement `reIndent=false` in Renderer**: consume the leading
      Whitespace run once at the top of `renderStatement()`. If
      `reIndent=false`, concatenate their `text` and emit as indent;
      otherwise emit `emitIndent(indent)`. Add targeted tests (preserved
      indent, odd/mixed indent, zero indent, Sub body with nonstandard
      indent).

- [x] **8. Implement `reFormat=false` in Renderer**: after leading-indent
      handling, if `reFormat=false`, emit every remaining token verbatim;
      suppress the trailing `\n` when the last token already is a Newline.
      Tests: inter-token spacing preserved, continuation newlines preserved,
      unusual spacing like `x=  +  1`.

- [x] **9. Scanner: skip colon-splitting under `reFormat=false`**: keep
      `Colon` as a normal token in the segment; dispatch still uses first
      significant keyword, so block detection is unaffected. Tests for
      `x=1 : y=2` preserved on one line; `If x Then : y=1 : End If`
      preserved. Added `hasBlockCloserAfterFirst()` guard so lines like
      `For i = 1 To 10 : Print i : Next` don't open a phantom block.

- [x] **10. Scanner: preserve blank-line count under `reFormat=false`**: the
      existing scanner already emits one `BlankLineNode` per extra newline
      (2+ newlines → N-1 BlankLineNodes). Only the renderer needed a change:
      skip the "collapse multiple blanks to 1" branch when `reFormat=false`.

- [x] **11. Renderer: skip auto blank-line-between-definitions when
      `reFormat=false`** (`renderNodes`). Tests: adjacent Subs with no blank
      line between them stay adjacent.

- [x] **12. `anchoredPP` × `reIndent=false` interaction**: when
      `reIndent=false`, bypass `renderAnchoredPP` and fall back to
      leading-whitespace echo. Document and add a test.

- [x] **13. Round-trip test**: with `{reIndent=false, reFormat=false}`,
      source → format → output must equal input for a handful of fixtures
      (incl. mixed tabs, continuations, PP, colons, blank runs). Added 6
      `RoundTrip_*` tests; all pass. This is the acceptance gate.

- [ ] **14. FormatDialog wiring** *(deferred — confirm before starting)*: the
      existing "reindent" checkbox currently toggles "format vs
      pass-through". Decide whether to split it into two checkboxes or leave
      untouched until the core behavior is verified.

---

## Notes

- Mark each step complete by changing `[ ]` → `[x]` as we finish it.
- Each step should leave the tree in a buildable state with all tests green
  before moving on.
- Add any mid-stream decisions or deviations to a "Notes" subsection under
  the relevant todo.
