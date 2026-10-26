# Design

## Context

The formatter, the Sub/Function browser and intellisense share one pipeline (see proposal.md, Why):

```
FBSciLexer (styles) --> StyleLexer (tokens) --> TreeParser + TreeBuilder (ProgramTree)
                                                      |                  |
                                               ReFormatter/Renderer   SymbolTable (function list)
```

Root causes, confirmed by reading the code:

1. **Dots in `#define`.** `.` has no character class (`CharCategory.hpp`: `'.'` is `0`), so `FBSciLexer::lexPreprocessor` matches none of its branches. The character stays styled `Preprocessor`. `StyleLexer::emitPreprocessor` treats any `Preprocessor` run that doesn't start with `#` as whitespace and emits it through `emitWhitespaceRun`, so the `.` becomes a `Whitespace` token. The renderer then normalises the whitespace and the dot is lost.
2. **Split closers.** `TreeBuilder::closeBlock` won't close past a PP block. A `Loop` inside `#if` is therefore kept as a statement, which is fine. But nothing records that the branch closed the outer `Do`. At `#endif` the `Do` is still open, so `End Sub` pops the `Do` and every later `Sub` nests inside. `closeBlock` also calls `closeBranch()` first, which ends an `#else` branch when a closer appears inside it.
3. **Single-line `If` colons.** `TreeParser::processLine` splits at every `:` when `reFormat` is on, without checking for a single-line `If … Then a : b`.

## Goals / Non-Goals

**Goals:**
- Fix each bug at its source, and add a safety net so the formatter can never turn non-whitespace text into whitespace.
- Keep the fix structural. No `#if` condition evaluation is added to `TreeParser`.

**Non-Goals:**
- Changing how the renderer indents PP blocks. Option B needs no renderer change.
- Uneven branch closers (covered by the "Uneven preprocessor branch closers" requirement, which keeps the current behaviour).

## Decisions

### D1: Style `.` / `..` / `...` as Operator inside PP bodies (FBSciLexer)
Add a `.` branch to `lexPreprocessor`, just before the final `isOperator` check (the `.`-followed-by-digit number case is already handled earlier). It sets the `Operator` state and groups `..` / `...` the same way the default state does.
- *Alternative:* give `'.'` the `Operator` class in `CharCategory`. Rejected because `isOperator` is used for other decisions, for example `m_fieldAccess = !isOperator(chNext)` and the handling of `_` continuations. Changing the class globally could affect field-access and keyword styling outside PP bodies.
- Side effect: the editor now colours `.` in macro bodies as an operator. That is consistent with how other operators are coloured there.

### D2: `emitPreprocessor` never emits non-whitespace as Whitespace (StyleLexer)
For a run that doesn't start with `#`, split it into whitespace stretches, which go to `emitWhitespaceRun`, and non-whitespace stretches, which go through the same matcher `emitOperator` uses, so they come out as `Operator` tokens. This is defence in depth: any other character class 0 that reaches this path is kept instead of deleted.
- *Alternative:* fix only D1. Rejected because the deletion would come back with the next character the lexer forgets to handle.

### D3: Track closers per branch in a PP frame (TreeParser)
Replace `std::vector<std::size_t> m_ppDepths` with a stack of frames:

```
struct PpFrame {
    std::size_t depth;               // builder depth when #if opened (as today)
    std::vector<std::size_t> closes; // refused-closer count of each finished branch
    std::size_t current = 0;         // refused closers in the current branch
    bool hasElse = false;
};
```

- `TreeBuilder::closeBlock()` returns `bool`: `true` when it popped a block, `false` when it refused and emitted a statement. In `TreeParser`, when a closer is refused and a PP frame is open, `current` is incremented.
- On `#else` / `#elseif*`: push `current` onto `closes` and reset it. Only `#else` sets `hasElse`.
- On `#endif`: push `current` onto `closes`, then close the PP block as today. If `hasElse` is set and every entry equals the same `N > 0`, call a new `TreeBuilder::closeOuterBlocks(N)`. It repeats the pop part of `closeBlock` (close the branch, then pop) up to `N` times, stops at any PP entry, and attaches no closer. In every other case nothing more happens, which is the current behaviour.
- `#macro` frames share the stack but never get branches, so their closes list never qualifies.
- *Alternative:* re-parse each branch on its own and merge the results. Rejected as too heavy: the tree has no place for a block with several closers.
- *Alternative:* attach the per-branch closers to the `Do` as its closer. Rejected because the user chose option B (closers render with the branch), and the `ProgramTree` shape would have to change.

The closed outer blocks have no `closer`. The renderer and `TreeBuilder::finish()` already handle closer-less blocks, since an auto-closed block at end of file is the same case.

### D4: A refused closer does not end a PP branch (TreeBuilder)
In `closeBlock`, if the top entry is a branch and the entry below it has `isPP`, don't call `closeBranch()`. Emit a statement and return `false`. Code branches (`Else`, `Case`) keep today's behaviour, where `End If` closes the `Else` branch and then the `If`.

### D5: Single-line `If` keeps its colons (TreeParser)
In `processLine`, before splitting at a `Colon`, check whether the segment collected so far is a single-line `If`. That means `firstKeyword() == If`, and the segment contains `Then` followed by at least one significant, non-comment token. If it is, the colon is kept in the segment, and splitting stays off until the logical line ends. A segment with nothing after `Then` (block `If`) still splits, so `ColonSplitsIntoBlock` keeps passing.
- The colon then reaches `Renderer` spacing. The default rule (space on both sides) gives ` : `. Only the comment saying "never reaches here" needs updating.

### D7: Keep the space between a macro name and `(` (Renderer)
Found while implementing task 1.3. The spacing rule "no space before `(` after a name" turned `#Define LEFT1 (sprite1.x)` into `#Define LEFT1(sprite1.x)`. That changes an object-like macro into a function-like one. In a `#define` / `#macro` line, when the source separates the macro name from `(` with whitespace, the renderer now emits one space. This applies in both normal and anchored PP mode. An attached `(` is unchanged.
- *Alternative:* special-case it in `needsSpaceBefore`. Rejected because that function sees only two tokens, and the source gap is a whitespace token it never receives.

### D6: TDD order
For each bug, write the failing tests at the lowest layer first (lexer styling, then tokens, then tree, then formatter and symbols), run them and confirm they fail for the expected reason, then apply the decision above.

## Risks / Trade-offs

- [D3 closes outer blocks that have no closer token] → `SymbolTable` brace and keyword matching (`matchBranchGroup` and similar) may find no closer for that `Do`. Add a `SymbolTableTests` case to check that the function list and scopes are correct. Missing keyword-match highlighting for that one `Do` is acceptable.
- [Counting is structural, not by keyword: `#if … End If … #else … Loop … #endif` would count as balanced] → This is rare and wrong code. The result is still a well-formed tree, and it is no worse than today.
- [D5 single-line `If` detection with comments or line continuations] → Comments are skipped and continuation newlines are already part of the segment. Add a test with a trailing `'` comment.
- [D1 changes editor colouring] → This is the intended behaviour. It is covered by an `FBSciLexerTests` case.
