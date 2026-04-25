# Lexer Unification — TODO

Indexed task list derived from `LEXER_UNIFICATION_PLAN.md`. Implement iteratively, reference by `T<n>`.

## Phase 0 — Preparation

- [x] **T1** — Lift `third_party/lexilla/test/TestDocument.{h,cxx}` into `src/lib/analyses/lexer/MemoryDocument.{hpp,cpp}`. Add to `fbide_lib`. Drop from `tests/CMakeLists.txt`.
- [x] **T2** — Make `FBSciLexer::LineState` public. Move out of `private:` (or hoist to `editor/lexilla/FBLineState.hpp`). Verify `bit_cast` round-trip preserved.
- [x] **T3** — Move `structuralKeywords` and `ppKeywords` maps from `Lexer.cpp` to new `analyses/lexer/KeywordTables.{hpp,cpp}`. Old Lexer keeps using them.
- [x] **T4** — Move `annotateVerbatim`, `classifyCommentPragma`, `matchWordIgnoreCase`, `skipSpaces` from `Lexer.cpp` to new `analyses/lexer/VerbatimAnnotator.{hpp,cpp}`.
- [x] **T5** — Add `ThemeCategory style = ThemeCategory::Default` field to `Token` in `analyses/lexer/Token.hpp`. No consumer updates.

## Phase 1 — New Lexer

- [ ] **T6** — Add `IStyledSource` abstract interface in `analyses/lexer/StyledSource.hpp`. Methods: `length`, `styleAt`, `getCharRange`, `lineFromPosition`, `lineState`.
- [ ] **T7** — Implement `MemoryDocStyledSource` adapter wrapping `MemoryDocument&`. Forwards to `IDocument::*`.
- [ ] **T8** — Implement `WxStcStyledSource` adapter wrapping `wxStyledTextCtrl&`. Use `GetTextRangeRaw` for UTF-8 char extraction (no conversion alloc) — see also T34.
- [x] **T9** — ~~Rename old Lexer~~. Deviation: introduce new lexer as `StyleLexer` alongside old `Lexer`. Avoids 1024-line LexerTests rename diff. Old class stays named `Lexer` until T37 cleanup.
- [x] **T10** — Add new `StyleLexer` class skeleton in `analyses/lexer/StyleLexer.{hpp,cpp}`. Constructor takes `IStyledSource&`. `tokenise() -> std::vector<Token>` body just calls `nextStyle()` loop.
- [x] **T11** — Implement `nextStyle()` style-run coalescer. Returns `std::optional<StyleRange>`.
- [x] **T12** — Add `OperatorKind::Other` enum value to `Token.hpp`. No usages yet.
- [x] **T13** — Implement `emitDefault` (whitespace/newline splitter). Handle `\r`, `\n`, `\r\n`. Reset `m_canBeUnary = true` on newline.
- [x] **T14** — Implement `emitOperator` first-char switch. `+`/`-`/`*`/`@` unary disambig via `m_canBeUnary`. Closing brackets → `m_canBeUnary = false`.
- [x] **T15** — Implement `emitIdentifier` (no `structuralKeywords` fallback; KeywordKind::None always).
- [x] **T16** — Implement `emitKeyword` with `structuralKeywords` text lookup → `KeywordKind`.
- [x] **T17** — Implement 1:1 emitters: Number, String, StringOpen, Comment, MultilineComment, Label, Error.
- [x] **T18** — Implement `emitPreprocessor` within-line coalescer (KeywordPP starts; subsequent Preprocessor runs append; newline ends).
- [x] **T19** — Wire `Token::style` assignment in every emitter per the plan's per-style table.
- [x] **T20** — Run `annotateVerbatim` (T4) post-pass at end of `tokenise()`.
- [x] **T21** — Added `tests/StyleLexerTests.cpp`. 23 tests covering Default/Operator splitters, Identifier (no fallback), Keyword (with fallback), PP coalescing, 1:1 emits, field-access (the motivating bug), arrow-access, line-continuation propagation, verbatim annotation. 372 total tests green.

## Phase 2 — Parity

- [x] **T22** — ~~Token-stream diff harness~~. Deviation: skipped formal parity rig in favour of targeted samples (T24) + audit (T25/T26). The two lexers will never be byte-for-byte equal (OperatorKind shrinkage, REM=Comment, Label split). Behavioural tests cover the cases that matter.
- [x] **T23** — ReFormatterTests will run against new lexer at consumer-switch time (T32). Audit (T25) confirms only `Colon` and `ParenOpen` are needed from OperatorKind; new lexer emits both correctly.
- [x] **T24** — Added 6 edge-case parity tests in `StyleLexerTests`: field-access (`obj.integer`, `obj->integer`, `this _\n . _\n integer()`), `asm/end asm`, REM-as-comment, nested `/'...'/`, CRLF line endings, number underscore (Error).
- [x] **T25** — Audited. Production formatter only branches on `Colon` (ReFormatter.cpp:102) and `ParenOpen` (ReFormatter.cpp:389). All other granular OperatorKinds (compound assigns, shifts, comparisons) are exercised only by old LexerTests. New `Other` collapse is safe.
- [x] **T26** — Audited. `KeywordKind::Rem` has zero consumers. Dead enum value, removed at T37/T38 cleanup.
- [x] **T27** — Verified: FBSciLexer styles `name:` as one Label run including the colon. emitFromRange splits into Identifier + Operator(Colon) for ReFormatter parity.
- [x] **T28** — Verified: FBSciLexer styles `1_000` as Error. Legacy lexer was over-permissive. Editor already shows this — no behaviour change.

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
