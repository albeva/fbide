# Design

## Context

See proposal.md (Why) for the symptoms. The mechanics were checked against current `develop` by running `fbide format` and reading the code:

```
FBSciLexer: a byte >= 0x80 is neither operator nor identifier -> stays Default style
StyleLexer::emitDefault:  each such byte -> its own Invalid token        (code)
StyleLexer::emitOperator: matchOperator default {Other, 1} per byte      (PP body, via the
                                                                          emitPreprocessor safety net)
Renderer::needsSpaceBefore: "everything else -> space" between those tokens
  --> spaces inside a UTF-8 sequence --> invalid UTF-8
PlainTextRenderer: wxString::FromUTF8(invalid) == ""        (wx returns empty on bad input)
QuickFormat::run:  editor.SetText("")                      --> document wiped (undoable)
FormatDialog:      preview is empty; onApply returns early  --> blank preview, no wipe
FormatCommand:     emits "" and exits 0                     --> silent empty output
```

Item 2: `matchOperator` reads `&=` greedily, then `>` becomes a separate `Other` token, and the default spacing rule separates them.

## Goals / Non-Goals

**Goals:**
- Fix the two root causes in the lexer and renderer.
- Add one independent safety check so a future formatter bug can't destroy a document.

**Non-Goals:**
- Styling non-ASCII identifiers as identifiers (FreeBASIC doesn't allow them anyway). They stay `Invalid` tokens and only need to survive unchanged.
- Changing the editor's own lexing or colouring (`FBSciLexer` is untouched).

## Decisions

### D1: Group non-ASCII bytes into one token (StyleLexer)
In `emitDefault`'s Invalid branch, and in `emitOperator` when `matchOperator` falls to its default, consume a whole run of bytes `>= 0x80` as one token. That is `Invalid` in `emitDefault` and `Other` in `emitOperator`, matching their current kinds. A run can't end in the middle of a character, since continuation bytes are also `>= 0x80`.
- *Alternative:* decode UTF-8 and emit one token per character. Rejected: per-character tokens would still get spaces between them, and the run is what we want to keep together.
- *Alternative:* teach `FBSciLexer` that non-ASCII bytes are identifier characters. Rejected: that changes editor colouring and symbol parsing, which is a bigger change than the bug needs.

### D2: Keep source spacing next to unrecognised tokens (Renderer)
`Invalid` tokens and non-ASCII `Other` tokens, next to anything, get a space only if the source had whitespace between them. `needsSpaceBefore` sees only `(prev, curr)`, so the statement loops pass a flag saying "source had layout between prev and curr". They already skip layout tokens, so they can set it as they go. The same flag is used by D3.
- *Alternative:* always-space. Rejected because it turns `x变量` into `x 变量`.

### D3: Glued binary-operator pairs stay glued (Renderer)
If `prev` and `curr` are both operator tokens, the source had no layout between them, and both are "plain binary" operators, then no space. Plain binary means `OperatorKind` is `Other` or `Assign`, and the token is not one of `Negate`, `UnaryPlus`, `Dereference`, `AddressOf`, a paren, bracket or brace, `Comma`, `Semicolon`, `Colon`, `Dot`, `Arrow` or `TypeSuffix`. This keeps `&=` + `>` as `&=>`. `a=-1` and `a<=-1` are unaffected because the minus is `Negate`. `f(x)=1` is unaffected because `)` is a bracket.
- *Alternative:* add `&=>` to `matchOperator` as one token. Rejected: it isn't verified as a FreeBASIC operator, and every other unknown glued run would still be split. A structural rule covers them all.

### D4: Shared result check before applying (format module)
A new free function in `src/lib/format/` (for example `FormatCheck.hpp/.cpp`):

```
[[nodiscard]] auto isSafeFormatResult(std::string_view source, std::string_view formatted) -> bool;
```

- It returns false if `formatted` is empty while `source` has non-whitespace, or if `formatted` isn't valid UTF-8.
- It compares the two strings with whitespace removed (space, tab, CR, LF). `:` is skipped only where the output dropped it (statement splitting). Other bytes are compared after ASCII case folding, which allows for the case transform.
- It works on UTF-8 `std::string` from the token text, before conversion to `wxString`. So the renderers and transforms gain a step that joins token text into a string (the code already exists in `PlainTextRenderer`), and the check runs on that.

Callers:
- `QuickFormat::run`, for both the whole-document and the selection path: on failure, don't call `SetText` / `ReplaceSelection`; show `wxMessageBox(ctx.tr("dialogs.format.failed"), ...)`.
- `FormatDialog`: run the check when the preview is built. On failure, disable Apply and show the message in the preview area or in a message box when Apply is pressed.
- `FormatCommand::run`: on failure, `ConsoleOutput::writeError("fbide: formatting failed: <file>")`, write nothing, return `EXIT_FAILURE`.

- *Alternative:* only check for an empty result. Rejected: it wouldn't catch the earlier dropped-dot class of bug (text silently changed but still valid UTF-8).

### D5: Translation key
Add `[dialogs/format]` key `failed` to all 14 `resources/ide/locales/*.ini`. `Context::tr` returns `""` for a missing key and doesn't fall back to English, so every locale needs it. Use a short translation for each language, in the same style as the existing format strings.

### D6: TDD order
Write the failing tests first in each group (lexer tokens, then renderer output, then the check function, then callers), then fix.

## Risks / Trade-offs

- [The check rejects a legitimate format] → The user sees "Formatting failed" and keeps their text, which is safe. A test formats `tests/data/flappyfb.bas` and representative snippets with every option on and asserts the check passes.
- [The check costs one extra pass over the text] → It's linear and only runs on explicit format commands, not while typing.
- [D3 hides a real spacing improvement for some glued run] → Any such run is already either an operator the lexer doesn't know or invalid code. Leaving it as written is the safe choice.
- [Translation quality for 13 non-English locales] → The string is short. It can be refined by translators later.
- [Selection reformat compares a fragment] → Check the slice's source text against the fragment rather than the whole document.
