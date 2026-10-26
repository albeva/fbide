# Tasks

Each group follows TDD: write the tests, run them and confirm they fail for the stated reason, then fix and confirm they pass. Build with `cmake --build build/claude/debug`. Run a suite with `./build/claude/debug/tests/tests --gtest_filter='<Suite>.*'`. On macOS, never run `ui-tests` (CTest label `gui`).

## 1. Keep multi-byte characters in one token (design D1)

- [x] 1.1 Add failing `StyleLexerTests` cases: `Dim 变量 As Integer` yields exactly one non-layout token with text `变量`; `#define X 中` yields one token with text `中`; `x = 1 ： y = 2` yields one token with text `：`. Verify they fail because each byte is its own token.
- [x] 1.2 In `StyleLexer::emitDefault` (Invalid branch) and `StyleLexer::emitOperator` (default `matchOperator` case), consume a whole run of bytes `>= 0x80` as one token (D1). Verify 1.1 passes and that the `StyleLexerTests` and `FBSciLexerTests` suites still pass.

## 2. Renderer spacing for unrecognised text and glued operators (design D2, D3)

- [x] 2.1 Add failing `ReFormatterTests` cases for the spec's "Multi-byte characters survive formatting" and "No new spaces next to unrecognised text" scenarios: `Dim 变量 As Integer`, `#define X 中`, `x = 1 ： y = 2` (contains `：`, valid UTF-8), and `x变量 = 1` keeps `x变量`. Verify they fail.
- [x] 2.2 Add failing `ReFormatterTests` cases: `a &=> "-"` stays unchanged. Add regression guards that pass both before and after: `a=-1` gives `a = -1`, `a<=-1` gives `a <= -1`, `f(x)=1` gives `f(x) = 1`, `a&-1` gives `a & -1`. Verify the `&=>` case fails and the guards pass.
- [x] 2.3 Pass a "source had layout between prev and curr" flag from both statement loops into the spacing decision, and apply D2 (unrecognised tokens keep source spacing) and D3 (glued plain-binary operator pairs stay glued). Verify 2.1–2.2 and the whole `ReFormatterTests` suite pass.

## 3. Shared result check (design D4)

- [x] 3.1 Add `tests/unit/FormatCheckTests.cpp` (and register it in `tests/CMakeLists.txt`) with failing cases for `isSafeFormatResult`. It rejects an empty result for non-empty source, rejects invalid UTF-8, rejects a dropped `.` (`(a.b)` → `(ab)`), and rejects a changed identifier. It accepts whitespace-only changes, ASCII case changes, and `:` separators removed by splitting (`If x Then : Print x : End If` → three lines). Verify it fails to build or link before the function exists.
- [x] 3.2 Add `src/lib/format/FormatCheck.{hpp,cpp}` (registered in `src/lib/CMakeLists.txt`) implementing D4. Verify `FormatCheckTests` passes.
- [x] 3.3 Add a `FormatCheckTests` case that formats `tests/data/flappyfb.bas` and the 1.1 / 2.1 snippets through the full transform chain with every option on, and asserts the check passes. Verify it passes. This guards against false "Formatting failed" reports.

## 4. Use the check in every format entry point (design D4, D5)

- [x] 4.1 Add key `failed` under `[dialogs/format]` to all 14 `resources/ide/locales/*.ini`, translated per language. Verify with `grep -c '^failed=' resources/ide/locales/*.ini`, which should report 1 for each file.
- [ ] 4.2 In `QuickFormat::run`, for both the whole-document and the selection path, check before `SetText` / `ReplaceSelection`. On failure, leave the editor untouched and show the `dialogs.format.failed` message. Verify by running `fbide` and reformatting a document whose result fails the check (temporarily force failure in a debug build, or use a known-bad input from before the fix). The text is unchanged and the message appears.
- [ ] 4.3 In `FormatDialog`, run the check on the rendered result. On failure, Apply makes no change and the user is told formatting failed. Verify manually in the dialog as in 4.2.
- [x] 4.4 In `FormatCommand::run`, on failure write `fbide: formatting failed: <file>` to stderr, write no output, and return `EXIT_FAILURE`. Verify with `fbide format --reformat` on a forced-failure case (exit code non-zero, stdout empty). Then confirm that `Dim 变量 As Integer` now formats correctly with exit code 0.

## 5. Changelog and integration

- [x] 5.1 Add entries under `# Changes since 0.5.1` in `change-log.md`: formatting no longer deletes the document when it contains non-ASCII text, a failed format now keeps the original and says so, and glued operators such as `&=>` are no longer split. Verify the entries are present.
- [x] 5.2 Do a full build and run all unit tests: `cmake --build build/claude/debug`, then `ctest --test-dir build/claude/debug -LE gui --output-on-failure` on macOS (on Windows, run `ui-tests` too). Verify everything passes. Re-run the forum repro samples through `fbide format --reindent --reformat` and confirm each output matches its input layout with no bytes lost.

## Workflow follow-up

- Reply on the forum thread / issue #136 with the fixed items (1–4) and what is still open (5–7).
- Archive the change with `/opsx:archive` after review.
