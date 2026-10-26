# Proposal

## Why

Issue #136 reports two bugs that remain after the earlier formatter fixes (`6a67eae`). Formatting deletes the `.` from macro bodies such as `#Define LEFT1 (sprite1.x)`, which silently changes the program. A block closed separately in each `#if` / `#else` branch, for example `Do … #if … Loop Until a … #else … Loop Until b … #endif`, throws the parse tree off by one level. As a result, every later Sub/Function nests inside the wrong one in the function list, and the formatter indents everything after it wrongly. A third, closely related bug was found while investigating: the reformatter splits a single-line `If c Then a : b` at its colon. That moves `b` out of the condition.

## What Changes

- Formatting never drops or rewrites non-whitespace source text. `.` and `...` inside preprocessor directive bodies are recognised as operators, and the formatter keeps them as tokens.
- The shared block parser handles a block that is opened outside a preprocessor conditional and closed in every branch of it. After `#endif`, the outer block counts as closed, so the code that follows, and any later procedures, are at the correct level.
- The formatter keeps such closers at the indent level of the branch they sit in (the closer is indented like any other branch statement).
- A closer inside an `#else` / `#elseif` branch no longer ends that branch early.
- The reformatter no longer splits a single-line `If … Then …` statement at its colons. The whole line stays one statement. A block `If … Then : … : End If` is still split as today.
- The work is TDD-first: a failing unit test for each bug comes before its fix.

Out of scope:
- The other items in issue #136: `Extern` / `End Extern` completion, separate colours for compiler warnings and errors, and alignment of the compiler error columns.
- Evaluating `#if` conditions in the formatter or tree parser. The fix is about structure only.
- Uneven branch closers (for example `#if` closes a block but `#else` doesn't). These keep the current behaviour.

Classic-FBIde parity and legacy formats: the classic FBIde had no structural reformatter or symbol tree, so parity is unaffected. No `.fbt`, `.fbl`, `.ini`, `.lng` or `.fbs` format is touched.

## Capabilities

### New Capabilities
- `code-formatter`: what the Format command must guarantee about its output: source text is preserved, single-line statements keep their meaning, and indentation follows block structure, including inside preprocessor conditionals.
- `block-structure`: how source is turned into nested blocks (procedures, loops, `#if` regions). The formatter, the Sub/Function browser and intellisense all share this structure.

### Modified Capabilities
<!-- none: no specs exist yet -->

## Impact

- `src/lib/editor/lexilla/FBSciLexer.cpp`: styling of `.` / `...` in preprocessor bodies. This also changes editor syntax colouring of those characters, from preprocessor style to operator style.
- `src/lib/analyses/lexer/StyleLexer.cpp`: `emitPreprocessor` must not turn non-whitespace text into Whitespace tokens.
- `src/lib/analyses/parser/TreeParser.{hpp,cpp}`, `TreeBuilder.{hpp,cpp}`: per-branch closer tracking, the `#else`-branch closing fix, and the single-line `If` colon rule.
- `src/lib/format/transformers/reformat/Renderer.cpp`: a comment update only, since a colon can now reach the spacing rules.
- Consumers that benefit with no API change: the formatter (`ReFormatter`) and `IntellisenseService` → `SymbolTable` (the Sub/Function browser and symbols).
- Tests: `tests/unit/FBSciLexerTests.cpp`, `StyleLexerTests.cpp`, `ReFormatterTests.cpp`, `TreeParserTests.cpp`, `SymbolTableTests.cpp`.
- All three platforms are affected the same way. There is no platform-specific code.
