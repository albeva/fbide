# Tasks

Each group follows TDD: write the tests, run them and confirm they fail for the stated reason, then make the fix and confirm they pass. Build with `cmake --build build/claude/debug`. Run a single suite with `ctest --test-dir build/claude/debug -R <Suite> --output-on-failure`.

## 1. Dots in preprocessor bodies (design D1, D2)

- [x] 1.1 Add failing `FBSciLexerTests` cases: in `#define X (a.b)` the `.` is styled Operator, and in `#define V(a, ...) f(a)` the `...` is styled Operator as one run. Verify they fail because the dot is styled Preprocessor.
- [x] 1.2 Add failing `StyleLexerTests` cases: `#define X (a.b)` produces an Operator token with `OperatorKind::Dot` and text `.`, and no Whitespace token contains a non-whitespace character. Verify they fail.
- [x] 1.3 Add failing `ReFormatterTests` cases for the three macros from issue #136 (`#Define LEFT1 (sprite1.x)`, `#Define LEFT2 (sprite2.x)`, `#Define rng float32(myhash.v)`) and the `...` define, each expected unchanged. Verify they fail with the dot missing.
- [x] 1.4 In `FBSciLexer::lexPreprocessor`, style `.` / `..` / `...` as Operator (D1). Verify that 1.1–1.3 pass.
- [x] 1.5 In `StyleLexer::emitPreprocessor`, send non-whitespace stretches of a run that doesn't start with `#` through the operator matcher instead of `emitWhitespaceRun` (D2). Add a `StyleLexerTests` case that feeds such a run directly, or drives it with a character the lexer leaves Preprocessor-styled, and verify it now yields an Operator token.
- [x] 1.6 Keep the source space between a `#define` / `#macro` name and `(` (D7; found in 1.3). Verify with `ReFormatterTests.DefineKeepsObjectLikeMacroSpace`: object-like, function-like, `#macro` and anchored forms.

## 2. Single-line If keeps its colons (design D5)

- [x] 2.1 Add failing `ReFormatterTests` cases: `If t2 < t1 Then t1 -= 1 : t3 -= 1` stays one line; `If c Then a = 1 : b = 2 Else a = 3 : b = 4` stays one line with ` : ` spacing; the same with a trailing `' comment`. Verify the first two fail because the line is split.
- [x] 2.2 Add a `TreeParserTests` case: a single-line `If` with colons produces one statement node. Verify it fails.
- [x] 2.3 In `TreeParser::processLine`, stop splitting at colons once the segment is a single-line `If` (D5), and update the colon comment in `Renderer::needsSpace`. Verify that 2.1–2.2 pass and that `ColonSplitsIntoBlock` and `ColonSplitNestedBlock` still pass.

## 3. Closers split across preprocessor branches (design D3, D4)

- [x] 3.1 Add failing `TreeParserTests` cases (`block-structure` spec): `Sub A` with `Do … #if X … Loop Until p … #else … Loop Until q … #endif … End Sub`, then `Sub B … End Sub`, gives two top-level blocks; the full `delayHR` sample from issue #136 followed by another `Sub` gives two top-level blocks; and in `#else … Loop Until q … x = 1 … #endif`, `x = 1` is in the `#else` branch. Verify they fail (B nested in A, and `x = 1` outside the branch).
- [x] 3.2 Add a `TreeParserTests` case for uneven closers: `Do` / `#if X` / `Loop` / `#endif` / `Loop`. The final `Loop` closes the `Do`, and the inner one is a statement of the branch. Verify it passes both before and after the fix, so it guards against regressions.
- [x] 3.3 Add failing `SymbolTableTests` cases: the split-closer source above lists `A` and `B` as top-level procedures. Verify it fails.
- [x] 3.4 Add failing `ReFormatterTests` cases from the `code-formatter` spec: the `Do` closed in both branches renders with option-B indentation; a statement after `#endif` returns to Sub-body indent; and `End Sub` is at column 0. Verify they fail.
- [x] 3.5 Make `TreeBuilder::closeBlock` return `bool` (popped or refused), and don't close a PP branch on a refused closer (D4). Verify the `#else`-branch case from 3.1 passes and that existing `TreeParserTests` and `ReFormatterTests` still pass.
- [x] 3.6 Replace `m_ppDepths` with the `PpFrame` stack, count refused closers per branch, and add `TreeBuilder::closeOuterBlocks(n)` to apply them at `#endif` (D3). Verify that 3.1–3.4 pass.

## 4. Changelog and integration

- [x] 4.1 Add an entry under `# Changes since 0.5.1` in `change-log.md` (dots kept in `#define` bodies, `#if`-split loops no longer break the function list or indentation, single-line `If` with `:` no longer split, all #136). Verify the entry is present.
- [ ] 4.2 Do a full build and run all tests with `cmake --build build/claude/debug` and `ctest --test-dir build/claude/debug --output-on-failure`. Verify that everything passes (`tests` and `ui-tests`).

## Workflow follow-up

- Comment on issue #136 when the fix lands, then archive the change with `/opsx:archive`.
