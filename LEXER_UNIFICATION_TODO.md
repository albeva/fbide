# Lexer Unification — TODO

Indexed task list derived from `LEXER_UNIFICATION_PLAN.md`. Implement iteratively, reference by `T<n>`.

## Phase 0 — Preparation

- [ ] **T1** — Lift `third_party/lexilla/test/TestDocument.{h,cxx}` into `src/lib/analyses/lexer/MemoryDocument.{hpp,cpp}`. Add to `fbide_lib`. Drop from `tests/CMakeLists.txt`.
- [ ] **T2** — Make `FBSciLexer::LineState` public. Move out of `private:` (or hoist to `editor/lexilla/FBLineState.hpp`). Verify `bit_cast` round-trip preserved.
- [ ] **T3** — Move `structuralKeywords` and `ppKeywords` maps from `Lexer.cpp` to new `analyses/lexer/KeywordTables.{hpp,cpp}`. Old Lexer keeps using them.
- [ ] **T4** — Move `annotateVerbatim`, `classifyCommentPragma`, `matchWordIgnoreCase`, `skipSpaces` from `Lexer.cpp` to new `analyses/lexer/VerbatimAnnotator.{hpp,cpp}`.
- [ ] **T5** — Add `ThemeCategory style = ThemeCategory::Default` field to `Token` in `analyses/lexer/Token.hpp`. No consumer updates.

## Phase 1 — New Lexer

- [ ] **T6** — Add `IStyledSource` abstract interface in `analyses/lexer/StyledSource.hpp`. Methods: `length`, `styleAt`, `getCharRange`, `lineFromPosition`, `lineState`.
- [ ] **T7** — Implement `MemoryDocStyledSource` adapter wrapping `MemoryDocument&`. Forwards to `IDocument::*`.
- [ ] **T8** — Implement `WxStcStyledSource` adapter wrapping `wxStyledTextCtrl&`. Use `GetTextRangeRaw` for UTF-8 char extraction (no conversion alloc) — see also T34.
- [ ] **T9** — Rename existing `analyses/lexer/Lexer` class to `LegacyLexer` (file rename + symbol rename). All consumers stay green.
- [ ] **T10** — Add new `Lexer` class skeleton in `analyses/lexer/Lexer.{hpp,cpp}`. Constructor takes `IStyledSource&`. `tokenise() -> std::vector<Token>` body just calls `nextStyle()` loop.
- [ ] **T11** — Implement `nextStyle()` style-run coalescer. Returns `std::optional<StyleRange>`.
- [ ] **T12** — Add `OperatorKind::Other` enum value to `Token.hpp`. No usages yet.
- [ ] **T13** — Implement `emitDefault` (whitespace/newline splitter). Handle `\r`, `\n`, `\r\n`. Reset `m_canBeUnary = true` on newline.
- [ ] **T14** — Implement `emitOperator` first-char switch. `+`/`-`/`*`/`@` unary disambig via `m_canBeUnary`. Closing brackets → `m_canBeUnary = false`.
- [ ] **T15** — Implement `emitIdentifier` (no `structuralKeywords` fallback; KeywordKind::None always).
- [ ] **T16** — Implement `emitKeyword` with `structuralKeywords` text lookup → `KeywordKind`.
- [ ] **T17** — Implement 1:1 emitters: Number, String, StringOpen, Comment, MultilineComment, Label, Error.
- [ ] **T18** — Implement `emitPreprocessor` within-line coalescer (KeywordPP starts; subsequent Preprocessor runs append; newline ends).
- [ ] **T19** — Wire `Token::style` assignment in every emitter per the plan's per-style table.
- [ ] **T20** — Run `annotateVerbatim` (T4) post-pass at end of `tokenise()`.
- [ ] **T21** — Add `LexerAdapterTests.cpp` — per-emitter unit tests (Default split, Operator split, Identifier no-fallback, Keyword with fallback, PP coalesce, 1:1 emits).

## Phase 2 — Parity

- [ ] **T22** — Build parity test rig: harness that takes a source string, runs both `LegacyLexer` and new `Lexer` (via `MemoryDocStyledSource` + FBSciLexer), diffs token streams. Documented tolerances for OperatorKind shrinkage / KeywordKind::Rem absence / PP shape.
- [ ] **T23** — Run `ReFormatterTests` against new lexer behind a flag. Investigate every divergence; fix or document.
- [ ] **T24** — Add explicit parity samples: field-access `obj.foo`, field-access across `_` (`obj _\n .foo`, `obj _\n . _\n integer()`), `asm ... end asm`, `endasm` non-token, REM line, REM as comment, nested `/'...'/`, `\r\n` line endings.
- [ ] **T25** — Audit `OperatorKind` usages across `Renderer.cpp`, `TreeBuilder.cpp`, `ReFormatter.cpp`, `HtmlRenderer.cpp`, `PlainTextRenderer.cpp`. Decide which kinds collapse to `Other`. Update enum.
- [ ] **T26** — Audit `KeywordKind::Rem` usages. New lexer doesn't emit it (REM is `Comment`-styled). Migrate consumers to Comment-text detection or remove dead code.
- [ ] **T27** — Verify Label `:` boundary: does FBSciLexer include `:` in the Label run? Add `FBSciLexerTests` case if missing. Decide adapter shape (Identifier + `Operator(:)` pair, or single Identifier with `:` stripped).
- [ ] **T28** — Verify number `_` separator (`1_000`) parity. Old `Lexer` accepts `_` in number; FBSciLexer behavior — write test, confirm.

## Phase 3 — Switch consumers

- [ ] **T29** — Add `configureFbWordlists(FBSciLexer&, const Keywords&)` helper. Used by AutoIndent, FormatDialog, anywhere else needing a configured headless lexer.
- [ ] **T30** — Change `indent::decide` signature to `decide(const wxString& prevLine, const Keywords& kw)`. Drop empty-wordlist hack. Update single caller `CodeTransformer::applyIndentAndCloser`. Update `AutoIndentTests` to pass real keywords.
- [ ] **T31** — Switch AutoIndent internals to use new `Lexer` via `MemoryDocument` + `FBSciLexer` + `MemoryDocStyledSource`.
- [ ] **T32** — Switch `FormatDialog` / `ReFormatter` / `TreeBuilder` token construction site to new `Lexer`.
- [ ] **T33** — Switch `CaseTransform` callers (formatter pipeline) to new `Lexer`. CaseTransform itself unchanged (consumes `vector<Token>`).
- [ ] **T34** — `WxStcStyledSource::getCharRange` uses `wxStyledTextCtrl::GetTextRangeRaw` (returns UTF-8 `wxCharBuffer` without conversion alloc).
- [ ] **T35** — `CodeTransformer::applyWordCase` fast path: drop full lex, use `editor.Colourise(lineStart, wordEnd)` + `GetStyleAt(wordStart)`. Skip transform if not a keyword style. **Closes the field-access bug.**
- [ ] **T36** — `CodeTransformer::transformRange` paste path: use new `Lexer` via `WxStcStyledSource`. Drop `m_lexer`, `m_tokenBuffer`, `buildKeywordGroups`.

## Phase 4 — Cleanup

- [ ] **T37** — Delete `LegacyLexer` (`analyses/lexer/Lexer.cpp/hpp` old impl after T9 rename).
- [ ] **T38** — Delete `KeywordGroup`, `KeywordScope`, `tokenKindFor`, `scopeFor` from old `Lexer.hpp`.
- [ ] **T39** — Delete obsolete tests in `LexerTests.cpp` whose coverage moved to `FBSciLexerTests.cpp`. Keep adapter-specific cases.
- [ ] **T40** — Drop `third_party/lexilla/test/TestDocument.cxx` from `tests/CMakeLists.txt` (now lifted via T1).

## Out of scope / follow-up

- [ ] **T41** — Settings UI: warn before user deselects structurally-significant keywords (`if`, `then`, `end`, `endif`, etc.). Tracked separately.
- [ ] **T42** — Profile per-keystroke `FBSciLexer` construction cost in AutoIndent. If material, cache configured instance per Editor / Context, invalidate on settings change.
- [ ] **T43** — PP cross-`_` coalescing: extend `emitPreprocessor` to coalesce across newlines when `lineState(prev).continuePP` is set. Deferred — current per-line shape matches legacy.

## Acceptance gate

- All `LexerTests` / `FBSciLexerTests` / `ReFormatterTests` / `AutoIndentTests` green.
- Field-access typing test (`obj.integer`, `this _\n . _\n integer()`) does not re-case `integer` as Keyword2.
- No dependency from `analyses/lexer/` on the old char-by-char `next()`/`identifier()`/`number()`/`stringLiteral()`/etc. paths.
