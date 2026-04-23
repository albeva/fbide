# CodeTransformer Plan

Move auto-indent logic into a per-Editor `CodeTransformer` class and add
on-the-fly keyword case correction driven by a new `editor.keywordCase`
config option.

## Goal review

**What user wants:**

1. New config flag `editor.keywordCase` with values `None`, `Lower`,
   `Upper`, `Mixed` (capitalised — first char upper, rest lower).
2. As user types, on a word boundary look back at the just-finished
   word; if it is a real FB keyword, case-transform it in place.
3. Auto-inserted closers (`End If`, `Next`, ...) rendered with the
   same rule.
4. `None` means **no** transform when typing, and closer rendering
   defaults to **lowercase**.
5. `Decision::insertCloser` becomes a token list (so it can be re-cased
   per call), not a pre-baked `wxString`.
6. New class `CodeTransformer` owning indent + case logic, owned per
   Editor. `Editor::onCharAdded` delegates to it.
7. Reuse result vectors — avoid reallocating token buffers per
   keystroke.
8. Settings UI: dropdown in General tab, **directly under** the
   `autoIndent` checkbox.
9. Settings apply chain must trigger reload so new rule takes effect
   without restart.

## Architecture

**Reuse, don't duplicate**

- `CaseMode` already exists in
  `src/lib/format/transformers/case/CaseTransform.hpp`
  (`{ Mixed, Upper, Lower }`). Add `None`. Add stable parse / toString
  for INI persistence.
- `indent::decide()` stays a pure function. Only its `Decision` shape
  changes (closer becomes a token span).

**Keyword detection — Lexer, not a local cache**

`CodeTransformer` holds **one** `Lexer` instance built from the current
keyword groups (rebuilt in `applySettings`). On every word boundary it
re-lexes the line prefix into a reused `m_tokenBuffer`, finds the token
at the word position, and checks `isKeywordToken(kind)`. This naturally
handles strings, comments and asm scope — a flat string set cannot.

Cost per keystroke: one tokenisation of a single short line — µs.

**File layout**

- `src/lib/editor/CodeTransformer.{hpp,cpp}` — new class.
- `src/lib/editor/AutoIndent.{hpp,cpp}` — keep as pure indent decision;
  change `Decision::insertCloser` shape.
- `src/lib/format/transformers/case/CaseTransform.{hpp,cpp}` — extend
  enum with `None`, add parse / toString helpers.
- `src/lib/analyses/lexer/Lexer.hpp` — add
  `void tokeniseInto(const char*, std::vector<Token>&)` for vector
  reuse; keep `tokenise()` as a thin wrapper.

**`CodeTransformer` interface**

```cpp
class CodeTransformer final {
public:
    NO_COPY_AND_MOVE(CodeTransformer)

    explicit CodeTransformer(Context& ctx);

    /// Reload from config (autoIndent, keywordCase). Rebuilds the
    /// internal Lexer from the current keyword groups.
    void applySettings();

    /// Main entry from Editor's EVT_STC_CHARADDED handler.
    void onCharAdded(Editor& editor, int ch);

private:
    void applyIndent(Editor& editor);
    void applyWordCase(Editor& editor, int ch);
    auto transformWord(std::string_view word) const -> std::string;
    [[nodiscard]] auto renderCloser(std::span<const std::string_view> words) const -> wxString;

    Context& m_ctx;
    bool m_autoIndent = true;
    CaseMode m_keywordCase = CaseMode::Lower;
    std::unique_ptr<lexer::Lexer> m_lexer;
    std::vector<lexer::Token> m_tokenBuffer; // reused
};
```

**`Decision` change**

```cpp
struct Decision {
    int deltaLevels = 0;
    bool dedentPrev = false;
    std::span<const std::string_view> closerKeywords; // empty = none
};
```

`closerFor(KeywordKind)` returns a span into `constexpr` arrays
(`{"end","if"}`, `{"loop"}`, `{"next"}`, `{"end","sub"}`, ...). Words
stored lowercase; `CodeTransformer::renderCloser` applies the rule.

**Word-boundary trigger set**

Anything that is **not** an identifier / number character: space, tab,
newline, operators, parentheses, colon, semicolon, comma, brackets,
braces. Anything that closes the previous word.

**Settings UI**

`GeneralPage` left column, directly under `autoIndent` checkbox:

```
[x] Auto indent
Keyword case  [ Lower         v ]
```

Choice control populated from `CaseMode::all`. Locale keys:
`dialogs.settings.general.keywordCase`,
`.keywordCaseNone`, `.keywordCaseLower`,
`.keywordCaseUpper`, `.keywordCaseMixed`.

**Settings reload chain**

`SettingsDialog::applyChanges` already calls
`UIManager::updateEditorSettigs()`, which iterates documents and calls
`Editor::applySettings()`. We just need `Editor::applySettings()` to
forward to `m_codeTransformer->applySettings()` so the new case rule
and rebuilt Lexer take effect immediately.

## Out of scope

- Case-correcting keywords already in the buffer (handled by full
  Format command).
- Anticipatory transform mid-keyword (we wait for the boundary char).
- Preserving deliberate mixed case once the user opts into a rule.

## TODOs

1. Extend `CaseMode` with `None`. Add `parse()` / `toString()` for INI
   persistence. Update `CaseTransform::apply` to skip when `None`.
2. Add `editor.keywordCase` to `config_win.ini` / `config_linux.ini`
   / `config_macos.ini`, default `Lower`. Document allowed values in
   the comment block.
3. Change `Decision::insertCloser` → `closerKeywords:
   std::span<const std::string_view>`. Update `closerFor()` with
   `constexpr` arrays. Adapt `AutoIndentTests.cpp` accordingly.
4. Add `Lexer::tokeniseInto(const char*, std::vector<Token>&)` —
   refactor existing `tokenise()` to call it.
5. Create `src/lib/editor/CodeTransformer.{hpp,cpp}` with the
   interface above. Move indent-apply code from
   `Editor::onCharAdded` into `applyIndent`.
6. Implement `applyWordCase`:
   - Determine whether `ch` is a word-boundary character.
   - Compute word range `[start, end)` behind the cursor by scanning
     back over `_` / alnum chars in the buffer.
   - Lex the line prefix `[lineStart, end)` into `m_tokenBuffer`.
   - Find the token whose offset matches `start - lineStart`.
   - If `lexer::isKeywordToken(token.kind)` and
     `m_keywordCase != None`, replace the range with
     `transformWord(word)` via `SetTargetRange` / `ReplaceTarget`.
   - Restore caret to its prior position.
7. Implement `renderCloser` — joins lowercase words with single
   spaces, applies `m_keywordCase` (with `None` → lower default).
8. `Editor::applySettings` forwards to `m_codeTransformer->applySettings()`.
   `Editor::onCharAdded` delegates to
   `m_codeTransformer->onCharAdded(*this, event.GetKey())`. Drop
   `m_autoIndentEnabled` (gating moves into CodeTransformer).
9. `GeneralPage`:
   - Add `wxString m_keywordCase` member.
   - Read from `editor.keywordCase` with default `"Lower"`.
   - Add dropdown right under the `autoIndent` checkbox using the
     existing `choice()` helper.
   - Write back in `apply()`.
10. Locale: add label + 4 option strings to `en.ini` and `et.ini`.
11. Verify the apply chain — `SettingsDialog::applyChanges` →
    `UIManager::updateEditorSettigs` → `Editor::applySettings` →
    `CodeTransformer::applySettings`. Add the missing forward call
    if absent.
12. CMake: add `CodeTransformer.{cpp,hpp}` sources + headers.
13. Tests:
    - `CodeTransformerTests.cpp` for pure helpers (case-transform a
      word per `CaseMode`, render closer per mode, `None` → lower).
    - Extend `AutoIndentTests.cpp` for closer token spans.
14. Manual test matrix per `CaseMode`:
    - Type opener → closer cased correctly.
    - Type a real keyword (`if`, `print`, `sub`) → transformed at
      boundary.
    - Type word inside string / comment → untouched.
    - Switch case rule in settings → next keystroke uses new rule.
    - `None` → no transform on type; closer still lowercased.
15. Commit.
