# Proposal

## Why

Forum reports (follow-ups to issue #136) show two formatter bugs that are still present on current `develop`.

1. **Formatting can delete the whole document.** Reformatting a file that contains non-ASCII text outside strings and comments replaces the entire editor content with nothing. Examples are an identifier like `变量`, a character in a `#define` body, or a full-width `：`. The change can be undone, but it looks like random data loss. Reproduced with `fbide format --reformat` on `Dim 变量 As Integer`, `#define X 中` and `x = 1 ： y = 2`: all three produce 0 bytes of output.
2. **`a &=> "-"` becomes `a &= > "-"`.** The formatter inserts a space inside an operator sequence the user wrote with no gap, which changes the code.

## What Changes

- The lexer keeps each multi-byte UTF-8 character (and each run of them) in one token. Formatting never inserts whitespace inside a character.
- The formatter never inserts a space between tokens it does not understand (non-ASCII text) and their neighbours unless the source had one there.
- The formatter keeps two binary-operator tokens glued when they were glued in the source and neither is a unary or bracket operator (`&=` + `>` stays `&=>`).
- A safety check runs before any formatted result replaces the source. If the result is empty, or its non-whitespace text differs from the source (ignoring ASCII case and removed `:` separators), the original text is kept and the user sees a "Formatting failed" message. This applies to the Reformat command (editor and file browser), the Format dialog, and `fbide format` (error message and non-zero exit code).
- New translation key for the failure message, added to all 14 bundled locales.
- TDD: every fix starts with a failing test.

Out of scope:
- The other forum items: `&H`/`&O`/`&B` upper-casing (item 5), highlighting macro usages (item 6), and dialog flicker (item 7). Items 1 and 4 are already fixed.
- Supporting non-ASCII identifiers as real identifiers (styling or symbols). They only need to survive formatting unchanged.
- Recognising `&=>` as a FreeBASIC operator. Its exact meaning isn't verified here; the fix keeps any glued operator run as written.

Classic-FBIde parity and legacy formats: the classic IDE had no reformatter, so parity is unaffected. No `.fbt`, `.fbl`, `.ini` settings, `.lng` or `.fbs` format changes. The new key is added to the bundled locale `.ini` files only; no format change.

## Capabilities

### New Capabilities
- `code-formatter`: adds requirements that formatting never damages source text (multi-byte characters, glued operators) and never replaces the source with a failed result. The same capability is introduced by the in-flight change `fix-formatter-issue-136`; whichever is archived second adds its requirements to the spec the first one creates.

### Modified Capabilities
<!-- none: no specs exist yet in openspec/specs -->

## Impact

- `src/lib/analyses/lexer/StyleLexer.cpp`: group non-ASCII bytes in `emitDefault` and `emitOperator`.
- `src/lib/format/transformers/reformat/Renderer.cpp`: spacing rules for Invalid tokens and glued binary operators.
- New shared check in `src/lib/format/` used by `QuickFormat.cpp`, `FormatDialog.cpp` and `FormatCommand.cpp`.
- `resources/ide/locales/*.ini` (14 files): one new key.
- Tests: `tests/unit/StyleLexerTests.cpp`, `ReFormatterTests.cpp`, and a new unit test file for the safety check.
- All platforms are affected the same way. The message box is a standard wx dialog on each.
