# Lexer Unification Plan

## Goal

Single source of FB lexing rules across the IDE: replace `analyses/lexer/Lexer` (char-by-char scanner) with a thin **token producer** that walks style runs emitted by `FBSciLexer` (the Scintilla lexer). FBSciLexer becomes the only place that:

- decides what is keyword vs identifier vs comment vs string vs operator
- handles field-access propagation through `.` / `->` across `_` line continuation
- handles `asm ... end asm` scoping
- handles preprocessor continuation across `_`
- handles REM-as-comment
- handles nested multi-line comments

Token model (`analyses/lexer/Token.hpp`) stays mostly unchanged. **One additive change**: add `ThemeCategory style` field carrying the original FBSciLexer style class for the source range. Useful for debugging (assert TokenKind matches expected style) and reserved for a future feature. Default value `ThemeCategory::Default`. No existing consumer needs to read it. Downstream consumers (TreeBuilder, ReFormatter, AutoIndent, CaseTransform, renderers, CodeTransformer) keep their existing `Token` API.

## Architecture

### `IStyledSource` — interface

```cpp
namespace fbide::lexer {

class IStyledSource {
public:
    virtual ~IStyledSource() = default;
    [[nodiscard]] virtual auto length() const -> Sci_PositionU = 0;
    [[nodiscard]] virtual auto styleAt(Sci_PositionU pos) const -> ThemeCategory = 0;
    virtual void getCharRange(char* buffer, Sci_PositionU pos, Sci_PositionU len) const = 0;

    // Per-line state published by FBSciLexer (continueLine / continuePP /
    // fieldAccess / asmBlock / commentNestLevel). Used by adapter to decide
    // PP-line coalescing across `_`, etc.
    [[nodiscard]] virtual auto lineFromPosition(Sci_PositionU pos) const -> Sci_Position = 0;
    [[nodiscard]] virtual auto lineState(Sci_Position line) const -> FBSciLexer::LineState = 0;
};

}
```

**FBSciLexer::LineState becomes public.** Move the struct out of `private:` in `FBSciLexer.hpp` (or hoist to a sibling header `editor/lexilla/FBLineState.hpp` if we want to avoid forcing FBSciLexer.hpp inclusion everywhere). Bit-packed, `bit_cast`-able to/from `int` already.

Two adapters:

- **`MemoryDocStyledSource`** — wraps `MemoryDocument` (renamed from Lexilla `TestDocument`). Used by formatter / AutoIndent / CaseTransform / renderers (headless).
- **`WxStcStyledSource`** — wraps `wxStyledTextCtrl&`. Used by CodeTransformer (in-editor).

Both forward to underlying `Scintilla::IDocument::StyleAt` / `GetCharRange` / `Length` (or wxSTC equivalents).

### `MemoryDocument`

Lift `third_party/lexilla/test/TestDocument.{h,cxx}` into `src/lib/editor/lexilla/MemoryDocument.{hpp,cpp}` (or `analyses/lexer/`). Implements `Scintilla::IDocument`. Headless. No wx dep.

CMake: add to `fbide_lib`, drop the `third_party/lexilla/test/` source from tests target.

### New `Lexer`

```cpp
namespace fbide::lexer {

struct StyleRange {
    ThemeCategory style;
    Sci_PositionU start;
    Sci_PositionU end;  // exclusive
};

class Lexer final {
public:
    explicit Lexer(IStyledSource& src);
    [[nodiscard]] auto tokenise() -> std::vector<Token>;

private:
    [[nodiscard]] auto nextStyle() -> std::optional<StyleRange>;
    [[nodiscard]] auto stringFromRange(Sci_PositionU start, Sci_PositionU end) const -> std::string;
    void emitFromRange(StyleRange range, std::vector<Token>& out);

    // Per-style emitters (push 1 or more Tokens into `out`):
    void emitDefault(StyleRange range, std::vector<Token>& out);
    void emitOperator(StyleRange range, std::vector<Token>& out);
    void emitIdentifier(StyleRange range, std::vector<Token>& out);
    void emitKeyword(StyleRange range, TokenKind kind, std::vector<Token>& out);
    void emitPreprocessor(StyleRange range, std::vector<Token>& out);
    // Number, String, StringOpen, Comment, MultilineComment, Label → 1:1.

    IStyledSource& m_src;
    Sci_PositionU m_pos = 0;
    bool m_canBeUnary = true;
    bool m_inPpLine = false;       // for KeywordPP+Preprocessor coalescing within a line
    KeywordKind m_ppKind = KeywordKind::None;
};

}
```

`tokenise()` loop:

```cpp
auto Lexer::tokenise() -> std::vector<Token> {
    std::vector<Token> out;
    out.reserve(m_src.length() / 5);
    while (auto r = nextStyle()) emitFromRange(*r, out);
    annotateVerbatim(out);
    return out;
}
```

`nextStyle()` coalesces equal-style adjacent positions into a single range. **No** style-aware splitting — splitting happens in per-style emitters.

## Per-style emission rules

| Style                            | Tokens emitted                                                   |
|----------------------------------|------------------------------------------------------------------|
| `Default`                        | Whitespace and Newline tokens, split on `\n`/`\r` and ws/non-ws |
| `Operator`                       | One Token per matched operator (longest-match table)            |
| `Identifier`                     | One Identifier Token, `KeywordKind::None` (no fallback — trust FBSciLexer's wordlists) |
| `Keyword1..4, Custom1/2, Asm1/2` | One keyword Token; KeywordKind from `structuralKeywords` text lookup  |
| `KeywordPP`                      | Starts a Preprocessor Token; remembers `KeywordKind::PpIf` etc. |
| `Preprocessor`                   | Body of PP Token (coalesced with prior `KeywordPP` on same line); split at newlines to keep "one Token per physical line" semantics |
| `Number`                         | One Number Token                                                |
| `String`                         | One String Token                                                |
| `StringOpen`                     | One UnterminatedString Token                                    |
| `Comment`                        | One Comment Token                                               |
| `MultilineComment`               | One CommentBlock Token                                          |
| `Label`                          | One Identifier Token (label is identifier-like for formatter)   |
| `Error`                          | Invalid Token                                                   |

### Default-range splitter

```cpp
void Lexer::emitDefault(StyleRange r, std::vector<Token>& out) {
    char buf[64];
    auto pos = r.start;
    while (pos < r.end) {
        const auto chunk = std::min<Sci_PositionU>(sizeof(buf), r.end - pos);
        m_src.getCharRange(buf, pos, chunk);
        // Walk buf, slice on:
        //   '\n' / '\r'  → Newline Token (handles \r\n pair)
        //   ' '/'\t' run → Whitespace Token
        // Emit tokens, advance pos.
    }
}
```

Newline must reset `m_canBeUnary = true` (statement start), and reset `m_inPpLine` if no continuePP (FBSciLexer.cpp:193 implies multi-line PP — but since FBSciLexer keeps Preprocessor style across the newline only when `continuePP` was set, the next style range will still be Preprocessor; the splitter needs to emit a Newline boundary regardless).

### Operator-range splitter

Longest-match dispatch via first-char switch (faster than array scan or `unordered_map` at this scale — 18 entries, mostly 1-char):

```cpp
auto matchOperator(std::string_view slice) -> std::pair<OperatorKind, std::size_t> {
    using enum OperatorKind;
    if (slice.empty()) return {Other, 0};
    switch (slice[0]) {
    case '-':
        if (slice.size() >= 2 && slice[1] == '>') return {Arrow, 2};
        return {Subtract, 1};       // unary disambig at emit
    case '.':
        if (slice.starts_with("...")) return {Ellipsis3, 3};
        if (slice.starts_with(".."))  return {Ellipsis2, 2};
        return {Dot, 1};
    case ',': return {Comma, 1};
    case ';': return {Semicolon, 1};
    case ':': return {Colon, 1};
    case '?': return {Question, 1};
    case '(': return {ParenOpen, 1};
    case ')': return {ParenClose, 1};
    case '[': return {BracketOpen, 1};
    case ']': return {BracketClose, 1};
    case '{': return {BraceOpen, 1};
    case '}': return {BraceClose, 1};
    case '=': return {Assign, 1};
    case '+': return {Add, 1};
    case '*': return {Multiply, 1};
    case '@': return {AddressOf, 1};
    default:  return {Other, 1};   // <, <=, <<, ==, &, ^, \, +=, -=, etc.
    }
}
```

Everything not enumerated (`<`, `<=`, `<<`, `<<=`, `==`, `<>`, `>>`, `>>=`, `^`, `\`, `&`, `+=`, `-=`, `*=`, `/=`, `\=`, `^=`, `&=`, `#`, `$`, `%`, `!`) → `OperatorKind::Other` (NEW enum value, replaces granular kinds the formatter does not branch on). One byte consumed per fallback step — multichar operators not in the table get split into per-char `Other` tokens, but their text is preserved across the run via Token::text concatenation if the formatter cares (audit during Phase 2).

After emit, update `m_canBeUnary`:
- after closing brackets → `false`
- everything else → `true`

For `+`/`-`/`*`: flip to `UnaryPlus` / `Negate` / `Dereference` if `m_canBeUnary`.

**Breaking change**: Token's `OperatorKind` enum shrinks. Renderer/Reformatter currently switch on full set (e.g. `MulAssign`, `ShiftLeft`, `NotEqual`). Need to audit usages and collapse to `Other` where safe.

### Token's new `style` field

Every emitted Token carries `style = ThemeCategory` — the FBSciLexer style of the source range it came from. Trivial 1:1 cases (Identifier, Number, String, Comment, etc.) just copy `range.style`.

Aggregating cases:

- **Default-range splitter**: Whitespace and Newline tokens both get `style = Default`.
- **Operator-range splitter**: every emitted operator token gets `style = Operator`.
- **Preprocessor coalescer**: PP token spans KeywordPP + Preprocessor styles. Use `style = KeywordPP` when the line started with a directive run (recognised PP keyword); else `style = Preprocessor`. Keeps the "is this a known directive" signal without stuffing it into `KeywordKind` alone.

### Identifier / Keyword

Asymmetric on purpose. FBSciLexer already classified the run as Identifier vs Keyword via wordlist match — we trust that. Structural `KeywordKind` (If / Sub / End / etc.) is orthogonal to wordlist membership: it tells the formatter *what kind of block this opens / closes*, not *what colour to paint*. So:

- **Identifier**: no `structuralKeywords` lookup. Plain identifier.
- **Keyword**: text-based `structuralKeywords` lookup fills `KeywordKind` (FBSciLexer doesn't carry this — only style class).

```cpp
void Lexer::emitIdentifier(StyleRange r, std::vector<Token>& out) {
    auto text = stringFromRange(r.start, r.end);
    out.push_back(Token{
        TokenKind::Identifier,
        KeywordKind::None,        // trust FBSciLexer; no fallback
        OperatorKind::None,
        false,
        std::move(text)
    });
    m_canBeUnary = false;
}

void Lexer::emitKeyword(StyleRange r, TokenKind kind, std::vector<Token>& out) {
    auto text = stringFromRange(r.start, r.end);
    auto lower = toLower(text);
    auto kwKind = KeywordKind::Other;
    if (auto it = structuralKeywords.find(lower); it != structuralKeywords.end()) {
        kwKind = it->second;     // text → If / Sub / End / Then / ...
    }
    out.push_back(Token{ kind, kwKind, OperatorKind::None, false, std::move(text) });
    m_canBeUnary = true;          // matches current behavior (Lexer.cpp:612)
}
```

**Consequence**: if user removes a structural word (e.g. `endif`) from their keyword groups in settings, FBSciLexer styles it as Identifier and the formatter loses block dispatch for it. Acceptable — user's config, user's consequence. Default `fbfull.lng` ships full set; settings UI should warn before removing structurally significant words (separate task, not in scope here).

REM is styled `Comment` by FBSciLexer, so structural `KeywordKind::Rem` is unreachable from identifier path — currently emitted via word "rem" lookup in `analyses/lexer/Lexer::identifier` (Lexer.cpp:570). New behavior: REM region becomes a Comment Token; if any consumer relies on `KeywordKind::Rem`, audit and migrate to `TokenKind::Comment` detection.

### Preprocessor coalescing

```cpp
// On KeywordPP run:
m_inPpLine = true;
m_ppKind = lookupPpKeyword(textOfRange);  // PpIf, PpIfDef, PpEndIf, ...
emit Preprocessor Token starting here, KeywordKind = m_ppKind, text = text-of-range.

// On subsequent Preprocessor runs while m_inPpLine:
// Append text to the last emitted Preprocessor Token (or merge by extending range).

// On Newline within Default range while m_inPpLine:
m_inPpLine = false;
m_ppKind = KeywordKind::None;
```

**Cross-line continuation**: punted for now. Match current `analyses/lexer/Lexer::preprocessor` behaviour — one Token per physical line, no coalescing across `_`. TreeBuilder already lives with this.

When we later want to coalesce: detect via `IStyledSource::lineState(line).continuePP`, or use the simpler heuristic — **a line that starts with Preprocessor style but no `#` is a continuation**. FBSciLexer guarantees this by re-entering Preprocessor state at line start when `m_previousLineState.continuePP` is set (FBSciLexer.cpp:193). Either signal works; line-state read is more explicit.

### Newline / Whitespace classification details

Whitespace Token text is the literal whitespace (spaces / tabs). Newline Token text is the literal newline (`\n`, `\r`, or `\r\n`). Match `analyses/lexer/Lexer::whitespace` / `newline` exactly so renderers reproduce input verbatim.

## Removing `analyses/lexer/Lexer`

`Lexer.cpp` has logic worth preserving:
- `structuralKeywords` map → MOVE to `Token.hpp` (or new `KeywordTables.hpp`), reused by new lexer.
- `ppKeywords` map → MOVE same place.
- `annotateVerbatim` + `classifyCommentPragma` + `matchWordIgnoreCase` → MOVE to `VerbatimAnnotator.{hpp,cpp}`, reused by new lexer.
- `KeywordGroup`, `KeywordScope`, `tokenKindFor`, `scopeFor` → DELETE. New lexer doesn't take groups; FBSciLexer holds them via `WordListSet`.

After migration, `Lexer.{hpp,cpp}` files deleted. `Token.hpp` stays; new `Lexer.{hpp,cpp}` (style-walker form) replaces it under same path.

## Consumer migration

### CodeTransformer

```cpp
// Before tokenise: ensure styles are current
editor.Colourise(editor.PositionFromLine(line), wordEnd);
WxStcStyledSource src(editor.GetSTC());
Lexer lex(src);
auto tokens = lex.tokenise();
// or: just GetStyleAt(wordStart) for the case-on-type fast path.
```

For the case-on-type fast path, full tokenise is overkill. Two options:
1. Just check `GetStyleAt(wordStart)` is keyword-style; do case transform; skip otherwise. Doesn't need new lexer at all.
2. Use new lexer scoped to the line for consistency.

**Pick option 1** for hot path (per-keystroke). Keep new lexer for paste path (`transformRange`).

Removes: `m_lexer`, `m_tokenBuffer`, `buildKeywordGroups`, manual `isWordChar` walk, line-prefix re-tokenise, offset-matching loop.

### AutoIndent

`indent::decide(prevLine)` currently constructs `Lexer` with **empty wordlists** (AutoIndent.cpp:203) and relied on the old `structuralKeywords` fallback inside `Lexer::identifier`. Under the new rule (no fallback in `emitIdentifier`), empty wordlists would mean `if`/`end`/`sub` style as Identifier → no `KeywordKind` → `firstKeyword()` always None → indent always 0. **Breaks AutoIndent.**

Fix: `decide()` must receive real keyword wordlists. Signature change:

```cpp
auto indent::decide(const wxString& prevLine, const Keywords& kw) -> Decision {
    MemoryDocument doc;
    doc.Set(prevLine.utf8_string());
    FBSciLexer lex;
    configureFbWordlists(lex, kw);             // real wordlists, not empty
    lex.Lex(0, doc.Length(), Default, &doc);
    MemoryDocStyledSource src(doc);
    Lexer adapter(src);
    auto tokens = adapter.tokenise();
    ...
}
```

Caller (`CodeTransformer::applyIndentAndCloser`) passes `m_ctx.getConfigManager().keywords()`. AutoIndentTests update to pass a configured keyword list (use `fbfull.lng` data).

Per-call `FBSciLexer` instance allocates 9 `WordList` objects + runs `WordListSet` 9 times. Per-keystroke cost. Mitigation: cache a configured `FBSciLexer` per Editor (or per Context), invalidate on settings change. Benchmark before deciding whether to cache.

### CaseTransform

Already takes `const std::vector<Token>&` — no API change. Caller (FormatDialog or wherever) needs to switch from old lexer to new.

### Format pipeline (TreeBuilder, ReFormatter, FormatDialog)

`FormatDialog` calls into formatter which currently builds tokens via old Lexer. Switch the construction site to use FBSciLexer + MemoryDocument + new Lexer. TreeBuilder / ReFormatter consume `vector<Token>` unchanged.

Wordlist configuration: pull from `Context::getConfigManager().keywords()` (same data CodeTransformer used via `buildKeywordGroups`). Helper:

```cpp
void configureFbWordlists(FBSciLexer& lex, const Keywords& kw);
```

### Renderers (HtmlRenderer, PlainTextRenderer)

Consume Token. No change.

## Tests

### Move out
- `LexerTests.cpp` (1024 lines) — old lexer unit tests. Most assertions are about per-token shape (kind/keywordKind/operatorKind/text). Keep as parity harness for the new lexer; rename to `LexerAdapterTests.cpp`. Some tests will need updates for:
  - shrunk `OperatorKind` enum (collapse to `Other`)
  - PP token shape if we change it (default plan: no change)
  - REM tokenisation (becomes Comment, not KeywordKind::Rem)

### Stay
- `FBSciLexerTests.cpp` — already covers FBSciLexer itself. Add tests for any new responsibility (e.g., field-access state across continuation already covered).
- `ReFormatterTests.cpp` (1236 lines) — biggest parity surface. Run unchanged against new lexer; investigate every diff.
- `AutoIndentTests.cpp` (242 lines) — runs through `decide()`. Will exercise the new lexer end-to-end.

### Add
- Per-style-emitter unit tests (Default splitter, Operator splitter, PP coalescer).
- `WxStcStyledSource` round-trip (set text → colourise → tokenise → assert tokens).

## CMake changes

```diff
# src/lib/CMakeLists.txt
- analyses/lexer/Lexer.cpp
+ analyses/lexer/Lexer.cpp                        # rewritten
+ analyses/lexer/MemoryDocument.cpp
+ analyses/lexer/StyledSource.cpp                 # if non-trivial
+ analyses/lexer/VerbatimAnnotator.cpp
```

```diff
# tests/CMakeLists.txt
-     ${CMAKE_SOURCE_DIR}/third_party/lexilla/test/TestDocument.cxx
```

`fbide_lib` already pulls Lexilla (`lexilla_lexlib` / headers). No new third-party deps.

## Phase plan

### Phase 0 — preparation (1 PR)

- Lift `TestDocument` → `MemoryDocument` in `fbide_lib`.
- Move `structuralKeywords`, `ppKeywords`, `annotateVerbatim`, `classifyCommentPragma` out of `Lexer.cpp` into `KeywordTables.hpp` + `VerbatimAnnotator.{hpp,cpp}`. Old Lexer keeps using them. Tests stay green.

### Phase 1 — new Lexer (1 PR)

- Add `IStyledSource` + `MemoryDocStyledSource` + `WxStcStyledSource`.
- Add new `Lexer` (rename old → `LegacyLexer` temporarily).
- All current consumers continue using `LegacyLexer`.
- New tests `LexerAdapterTests.cpp` exercising new lexer directly.

### Phase 2 — parity (1 PR)

- Add a parity test rig: feed N source samples through both lexers, compare token streams (with documented tolerances for OperatorKind shrinkage / KeywordKind::Rem / PP shape).
- Run `ReFormatterTests` and `AutoIndentTests` against both lexers in parallel; fix divergences.

### Phase 3 — switch consumers (1 PR per consumer or batched)

- AutoIndent → new lexer.
- CaseTransform / FormatDialog / TreeBuilder / ReFormatter → new lexer.
- Renderers — no change (consume Token).
- CodeTransformer → STC styles directly for hot path (option 1); new lexer for paste path. Field-access bug closes here.

### Phase 4 — cleanup (1 PR)

- Delete `LegacyLexer`.
- Delete obsolete `KeywordGroup`/`KeywordScope`/`tokenKindFor`/`scopeFor`.
- Delete obsolete tests duplicating coverage now in FBSciLexerTests.

## Risk register

| Risk                                          | Mitigation                                                  |
|-----------------------------------------------|-------------------------------------------------------------|
| OperatorKind shrinkage breaks renderer logic  | Audit Renderer.cpp switches; collapse to `Other` carefully  |
| PP token shape change breaks TreeBuilder      | Default plan: keep one-Token-per-line; no change            |
| REM as Comment loses `KeywordKind::Rem`       | grep usages; if any, treat `Comment` text "rem ..." same way |
| Per-call FBSciLexer construction cost in AutoIndent | Profile; cache configured FBSciLexer per Editor/Context |
| Editor hot-path overhead of full tokenise     | CodeTransformer fast path skips tokenise (option 1)         |
| Style-byte read before colourise complete     | Force `Colourise(start, end)` before reading                |
| MemoryDocument missing `IDocument` calls FBSciLexer needs | Currently used by FBSciLexerTests so all calls covered |
| Lexilla `TestDocument` license / lift legality | Same project; check header — Neil Hodgson's Lexilla license permits |
| `_` continuation cases not exercised by parity tests | Add explicit `obj _ \n . _ \n integer()` sample          |
| Asm block boundary cases differ between lexers | Add parity samples for `asm/end asm`, `endasm` non-token   |
| Whitespace/Newline text exact-match for Renderer | Verify `\r\n` vs `\n` round-trip via PlainTextRenderer test |
| User removes structural keyword from group (e.g. `endif`, `then`, `end`) → AutoIndent / TreeBuilder lose block dispatch | Default `fbfull.lng` ships full set. Settings UI should warn before deselecting structurally significant words. Out of scope here — track as follow-up. |
| AutoIndent signature change breaks callers / tests | Single caller (`CodeTransformer::applyIndentAndCloser`) + `AutoIndentTests`; trivial fix |
| Multichar `Other` operator split into N×1-char tokens loses grouping (`<<=` → 3 tokens) | Audit formatter consumers — most `Other` ops only need spacing, not exact grouping. If grouping needed, coalesce adjacent `Other` tokens during emit. |

## Open questions before coding

1. **Where do `MemoryDocument` and `StyledSource` live?** Proposed: `src/lib/analyses/lexer/`. Rationale: same translation unit family as the new Lexer. Alternative: `src/lib/editor/lexilla/` next to FBSciLexer.

2. **Does `IStyledSource` need `lineState()`?** **Decided yes** — add now. Cheap to expose, useful for future PP-cross-`_` coalescing and for any debugging that needs to see FBSciLexer's bookkeeping. `FBSciLexer::LineState` becomes public. Adapters forward to `IDocument::GetLineState` / `wxStyledTextCtrl::GetLineState` and `bit_cast` to `LineState`.

3. **`OperatorKind::Other` semantics.** Token `text` already carries the literal. Anywhere the formatter currently switches on a specific `OperatorKind` (e.g. `MulAssign` for spacing), audit and choose: (a) keep the OperatorKind value, or (b) switch on text. Renderer audit needed in Phase 1.

4. **`KeywordKind::Other` for keyword tokens.** Current behavior (Lexer.cpp:144-149) emits `Other` only inside Code scope. New emit unifies — every keyword token gets text-based structural lookup. Slightly more permissive. Likely a non-issue but flag during parity.

8. **Identifier loses structural fallback.** Decided: yes, drop it. Trust FBSciLexer wordlists. Words like `endif` styled as Identifier when omitted from groups → no `KeywordKind`. AutoIndent gets real wordlists (signature change covers this). Fbfull.lng ships full structural set so default install unaffected.

9. **Closer for `decide()`** — `closerFor()` uses lowercase keyword text from `structuralKeywords` to render `end if` / `end sub`. Renderer applies case rule. Unaffected by lexer change.

5. **Label style.** FBSciLexer styles labels (`foo:` at line start) as `Label`. Old Lexer doesn't have a Label TokenKind — emits Identifier + Operator(`:`). For parity, new lexer maps Label range → Identifier Token (drop the `:`? include it?). Decision: emit Label as Identifier Token covering the name, then synthesize Operator(`:`) Token if `:` is part of the styled range. Verify with FBSciLexer.

6. **Asm block keyword classification.** FBSciLexer styles asm keywords as `KeywordAsm1/2`. Old Lexer routes through `m_asmKeywords`. New lexer just maps style → TokenKind; KeywordKind from text lookup. Need `structuralKeywords` to still classify `end` as `End` inside asm — currently done (table is global).

7. **Number with `_` separators.** Old Lexer accepts `_` inside numbers (Lexer.cpp:549). FBSciLexer? — verify in tests. If FBSciLexer styles `1_000` as Number throughout, parity holds. If it splits, parity breaks.

## Effort estimate

- Phase 0: 0.5 day
- Phase 1: 1.5 days
- Phase 2: 2 days (parity is the long pole)
- Phase 3: 1.5 days
- Phase 4: 0.5 day

Total: ~6 working days, dominated by parity validation.
