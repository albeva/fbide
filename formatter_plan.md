# Code Formatter Implementation Plan

## Overview

Replace the current `ReindentTransform` (single-pass token rewriter) with a proper
code formatter built on a lightweight formatting tree. The formatter does **not**
validate code — it makes best-guess structural decisions from tokenized input and
re-emits code with correct indentation and spacing.

Pipeline: **Lexer tokens → TreeBuilder → FormatNode tree → Renderer → output tokens**

**Design principle:** We only care about not breaking syntax, applying scoping
rules, and producing correct spacing. We do not validate or exactly parse the
language. Operator classification is a best-effort heuristic — when ambiguous,
pick whichever interpretation produces the same formatting output.

---

## Architecture

### Formatting Token

The analysis lexer (`lib/analyses/lexer/Lexer`) produces `lexer::Token` with
`TokenKind` and `KeywordKind`. The formatter needs finer operator classification.
Rather than wrapping tokens, we extend the lexer to produce richer information.

**Lexer changes:**

Add `OperatorKind` to classify operators during lexing. The enum covers every
FreeBASIC operator from the official documentation (freebasic.net/wiki/CatPgOpIndex).

Operators fall into two categories:
- **Symbol operators** — punctuation characters, lexed by the Lexer directly
- **Keyword operators** — words like `And`, `Mod`, `Not`, lexed as keywords (kw3 group)

Only symbol operators need `OperatorKind` classification. Keyword operators
(`And`, `Or`, `Mod`, `Shl`, etc.) are already identified by `KeywordKind` or
by being in the Keyword3 token group. The formatter's spacing rules can check
both `OperatorKind` (for symbol ops) and `TokenKind::Keyword3` (for keyword ops).

```cpp
enum class OperatorKind : std::uint8_t {
    None,            // not a symbol operator

    // Punctuation / structural
    ParenOpen,       // (   — call, index, grouping
    ParenClose,      // )
    BracketOpen,     // [   — string/pointer index
    BracketClose,    // ]
    BraceOpen,       // {   — initializer
    BraceClose,      // }
    Comma,           // ,
    Semicolon,       // ;   — Print separator
    Colon,           // :   — statement separator / label / bitfield
    Dot,             // .   — member access
    Ellipsis2,       // ..  — range (already recognized)
    Ellipsis3,       // ... — variadic (already recognized)
    Arrow,           // ->  — pointer member access
    Question,        // ?   — shorthand for Print

    // Assignment (symbol forms)
    Assign,          // =   (assignment context)
    AddAssign,       // +=
    SubAssign,       // -=
    MulAssign,       // *=
    DivAssign,       // /=
    IntDivAssign,    // \=
    ExpAssign,       // ^=
    ConcatAssign,    // &=

    // Comparison
    Equal,           // =   (comparison context)
    NotEqual,        // <>
    Less,            // <
    Greater,         // >
    LessEqual,       // <=
    GreaterEqual,    // >=

    // Arithmetic (binary)
    Add,             // +   (binary)
    Subtract,        // -   (binary)
    Multiply,        // *   (binary)
    Divide,          // /
    IntDivide,       // backslash
    Exponentiate,    // ^
    Concatenate,     // &   (string concatenation with conversion)

    // Arithmetic (unary)
    Negate,          // -   (unary)
    UnaryPlus,       // +   (unary)
    AddressOf,       // @   (address of)
    Dereference,     // *   (value of / pointer dereference)

    // Shift (symbol forms — << and >> when not keyword SHL/SHR)
    ShiftLeft,       // <<
    ShiftRight,      // >>

    // Type suffix sigils (context-dependent, not always operators)
    Hash,            // #   (string type suffix, also PP stringize)
    Dollar,          // $   (string type suffix, also non-escaped string prefix)
    Percent,         // %   (integer type suffix)
    Exclamation,     // !   (single type suffix, also escaped string prefix)
};
```

Add `OperatorKind operatorKind` field to `Token`. The lexer sets it during
tokenization using previous-token context to distinguish binary vs unary.

**Complete FreeBASIC operator reference (from official docs):**

*Symbol operators the lexer must handle:*

| Symbol | Kinds                    | Notes                                      |
|--------|--------------------------|--------------------------------------------|
| `=`    | Assign / Equal           | context: assignment vs comparison          |
| `+`    | Add / UnaryPlus          | binary after value, unary after operator   |
| `-`    | Subtract / Negate        | binary after value, unary after operator   |
| `*`    | Multiply / Dereference   | binary after value, unary after operator   |
| `/`    | Divide                   | also starts block comment `/'`             |
| `\`    | IntDivide                |                                            |
| `^`    | Exponentiate             |                                            |
| `&`    | Concatenate              | also starts number prefix `&h`, `&o`, `&b` |
| `@`    | AddressOf                | always unary                               |
| `<`    | Less                     | starts `<=`, `<>`, `<<`, `<<=`             |
| `>`    | Greater                  | starts `>=`, `>>`, `>>=`                   |
| `.`    | Dot                      | starts `..`, `...`                         |
| `(`    | ParenOpen                |                                            |
| `)`    | ParenClose               |                                            |
| `[`    | BracketOpen              |                                            |
| `]`    | BracketClose             |                                            |
| `{`    | BraceOpen                |                                            |
| `}`    | BraceClose               |                                            |
| `,`    | Comma                    |                                            |
| `;`    | Semicolon                | Print separator                            |
| `:`    | Colon                    | statement separator / label / bitfield     |
| `?`    | Question                 | shorthand for Print                        |
| `#`    | Hash                     | type suffix, PP stringize                  |
| `$`    | Dollar                   | type suffix, non-escaped string prefix     |
| `%`    | Percent                  | integer type suffix                        |
| `!`    | Exclamation              | single type suffix, escaped string prefix  |

*Compound symbol operators (must be lexed as single tokens):*

| Token  | Kind           | Notes                                      |
|--------|----------------|--------------------------------------------|
| `+=`   | AddAssign      |                                            |
| `-=`   | SubAssign      |                                            |
| `*=`   | MulAssign      |                                            |
| `/=`   | DivAssign      |                                            |
| `\=`   | IntDivAssign   |                                            |
| `^=`   | ExpAssign      |                                            |
| `&=`   | ConcatAssign   |                                            |
| `<>`   | NotEqual       |                                            |
| `<=`   | LessEqual      | careful: also prefix of `<<=`              |
| `>=`   | GreaterEqual   | careful: also prefix of `>>=`              |
| `<<`   | ShiftLeft      | careful: also prefix of `<<=`              |
| `>>`   | ShiftRight     | careful: also prefix of `>>=`              |
| `<<=`  | ShlAssign      | longest match from `<`                     |
| `>>=`  | ShrAssign      | longest match from `>`                     |
| `->`   | Arrow          |                                            |
| `..`   | Ellipsis2      | already recognized                         |
| `...`  | Ellipsis3      | already recognized, longest match from `.` |

*Keyword operators (already handled as keywords, no lexer changes needed):*

| Keyword    | Category         | Binary/Unary | Compound assign form |
|------------|------------------|--------------|----------------------|
| `And`      | Bitwise          | Binary       | `And=`               |
| `Or`       | Bitwise          | Binary       | `Or=`                |
| `Xor`      | Bitwise          | Binary       | `Xor=`               |
| `Eqv`      | Bitwise          | Binary       | `Eqv=`               |
| `Imp`      | Bitwise          | Binary       | `Imp=`               |
| `Not`      | Bitwise          | Unary        | —                    |
| `Mod`      | Arithmetic       | Binary       | `Mod=`               |
| `Shl`      | Shift            | Binary       | `Shl=`               |
| `Shr`      | Shift            | Binary       | `Shr=`               |
| `AndAlso`  | Short-circuit    | Binary       | —                    |
| `OrElse`   | Short-circuit    | Binary       | —                    |
| `Is`       | RTTI             | Binary       | —                    |

*Keyword compound assignments (`Mod=`, `And=`, `Shl=`, etc.):*

These are two tokens in the source: keyword + `=`. The lexer does NOT merge
them into a single token. Instead, the formatter recognizes the pattern
`<keyword-operator> =` and treats the `=` as part of the compound assignment
for spacing purposes (no space between `Mod` and `=`). This avoids the lexer
needing lookahead into keyword tokens.

**Binary vs unary detection (in the lexer):**

After an identifier, number, string, `)`, `]`, `}` → **binary** context.
After an operator (except `)`, `]`, `}`), keyword, `(`, `[`, `,`, or at
line/statement start → **unary** context.

Applies to these ambiguous symbols:
- `-` → Subtract (binary) or Negate (unary)
- `+` → Add (binary) or UnaryPlus (unary)
- `*` → Multiply (binary) or Dereference (unary)
- `@` → always AddressOf (unary, no binary form)

**`=` context (Assign vs Equal):**

Distinguishing assignment from comparison without full parsing is unreliable.
Both use the same `=` symbol. Heuristic: the formatter applies the same
spacing to both (space on each side), so exact classification is not critical.
The lexer defaults to `Assign` and the formatter does not differentiate.

### Formatting Tree

Variant-based tree with visitor pattern. Three node types, with `BlockNode`
behind `unique_ptr` for recursion.

```cpp
namespace fbide::format {

struct BlankLineNode {};

struct StatementNode {
    std::vector<lexer::Token> tokens;
};

struct BlockNode;
using Node = std::variant<BlankLineNode, StatementNode, std::unique_ptr<BlockNode>>;

struct BlockNode {
    std::optional<StatementNode> opener;   // "Sub Main()", "#ifdef DEBUG"
    std::vector<Node> body;                // statements, blank lines, nested blocks
    std::optional<StatementNode> closer;   // "End Sub", "#endif"
};

struct ProgramTree {
    std::vector<Node> nodes;
};

// Visit with std::visit + overloaded lambdas
template <typename Visitor>
void visitNode(const Node& node, Visitor&& visitor) {
    std::visit(std::forward<Visitor>(visitor), node);
}

}
```

Branches (Else, Case, #else) are child BlockNodes with an opener but no closer.
The renderer checks whether a child block's opener is a mid-block keyword and
emits it at the parent's indent level rather than indented.

Example tree for:
```
Sub Main
    #ifdef DEBUG
        Print "debug"
    #else
        Print "release"
    #endif
    Print "done"
End Sub
```

```
BlockNode(opener=[Sub Main], closer=[End Sub])
├── BlockNode(opener=[#ifdef DEBUG], closer=[#endif])
│   ├── StatementNode [Print "debug"]
│   └── BlockNode(opener=[#else])             ← branch, no closer
│       └── StatementNode [Print "release"]
└── StatementNode [Print "done"]
```

### Tree Builder

Stack-based, linear scan. The scanner iterates tokens and calls builder methods.
The builder maintains a stack of open `BlockNode`s and a token collection buffer.

```cpp
class TreeBuilder {
public:
    void append(const lexer::Token& token);
    void statement();      // collected → StatementNode in current block body
    void openBlock();      // collected → BlockNode opener, push onto stack
    void openBranch();     // close current branch, collected → new BlockNode opener
    void closeBlock();     // collected → BlockNode closer, pop and add to parent
    void blankLine();      // add BlankLineNode to current block body
    auto finish() -> ProgramTree;  // auto-close unclosed blocks, return root

private:
    std::vector<lexer::Token> m_collected;
    std::vector<std::unique_ptr<BlockNode>> m_stack;
};
```

The scanner drives the builder:

```
for each token:
    skip whitespace
    track blank lines (consecutive newlines → blankLine())

    if token is Preprocessor:
        handle PP (openBlock / openBranch / closeBlock based on KeywordKind)
    else if token is keyword:
        switch keywordKind:
            // Block openers
            Sub/Function/Do/While/For/...:
                collect tokens until newline → openBlock()
            If:
                collect until newline
                if last keyword was Then → openBlock()
                else → statement() (single-line)
            Type:
                collect until newline
                if second keyword is As → statement() (alias)
                else → openBlock()
            Select:
                collect until newline → openBlock()
            // Mid-block
            Else/ElseIf/Case:
                collect until newline → openBranch()
            // Closers
            End/Loop/Next/Wend:
                collect until newline → closeBlock()
            // Declare
            Declare:
                collect until newline → statement()
            // Other
            default:
                collect until newline → statement()
    else:
        collect until newline → statement()

    within a line:
        ':' colon → split: statement() the collected so far, continue
        '_' continuation → include in current statement, consume to next line
```

### Renderer

Walks the tree using `std::visit` and emits tokens with indentation and spacing.

```cpp
class Renderer {
public:
    Renderer(std::size_t tabSize, bool anchorHash);
    auto render(const ProgramTree& tree) -> std::string;

private:
    void renderNode(const Node& node, std::size_t indent);
    void renderBlock(const BlockNode& block, std::size_t indent);
    void renderStatement(const StatementNode& stmt, std::size_t indent);
    auto needsSpaceBefore(const lexer::Token& prev, const lexer::Token& curr) const -> bool;
    auto isMidBlockOpener(const StatementNode& opener) const -> bool;
};
```

Rendering rules:

**Indentation:**
- StatementNode: emit `indent * tabSize` spaces, then tokens with spacing.
- BlockNode opener: emit at current indent.
- BlockNode body: each child at indent+1.
- BlockNode closer: emit at current indent.
- Branch BlockNode (Else/Case/#else): opener emits at **parent's** indent level
  (not indented), body at parent indent+1. Detected by `isMidBlockOpener()`.
- PP blocks: track ppIndent separately. On PP open, save codeIndent. On PP close,
  restore codeIndent. Total indent = ppIndent + codeIndent.
- Anchored hash mode: `#` at column 0, directive indented by `indent * tabSize - 1`.

**Spacing (based on OperatorKind and TokenKind):**

The goal is correct, readable spacing — not exact syntax matching. We care about
not breaking meaning and applying consistent visual style.

- Binary symbol operators (`+`, `-`, `*`, `/`, `\`, `^`, `&`, `=`, `<>`, `<=`,
  `>=`, `<`, `>`, `<<`, `>>`): space on both sides (`a + b`, `x <> y`)
- Binary keyword operators (`And`, `Or`, `Mod`, `Shl`, etc.): space on both
  sides (`a And b`, `x Mod 2`)
- Compound assignments (`+=`, `-=`, `And=`, etc.): space on both sides
  (`x += 1`). For keyword forms (`Mod=`), no space between keyword and `=`.
- Unary operators (`-`, `+`, `*`, `@`, `Not`): space before, no space after
  (`= -3`, `*ptr`, `Not flag`)
- Parens/brackets: no space inside (`foo(x)`, `a[i]`, `(a + b)`)
- Comma: no space before, space after (`a, b`)
- Semicolon: space before and after (`Print a ; b`)
- Braces: space inside (`{ 1, 2 }`)
- Dot/Arrow: no space on either side (`foo.bar`, `ptr->x`)
- Keywords/identifiers: single space between (`Dim x As Integer`)
- Question mark: treat like keyword Print (`? "hello"` → `? "hello"`)
- Colon: handled during tree build (split into statements)
- Type suffixes (`#`, `$`, `%`, `!`): no space before when suffix on identifier
  (`a$`, `x%`). Context detection TBD — may need heuristic.

**Blank lines:**
- Preserve blank lines from source.
- Ensure at least 1 blank line between top-level compound block definitions
  (Sub, Function, Type, etc.).
- Collapse runs of 2+ blank lines to 1.

---

## Step-by-Step Implementation

### Phase 1: Lexer — compound operators and classification ✅

All steps complete. `OperatorKind` enum added to `Token.hpp`. Lexer produces
compound operators as single tokens with longest-match. Binary vs unary context
tracked via `m_canBeUnary`. 95 lexer tests passing (31 new).

1. ✅ Add `OperatorKind` enum to `Token.hpp`.
2. ✅ Add `OperatorKind operatorKind` field to `Token` struct.
3. ✅ Longest-match compound operator lexing (`<<=`, `>>=`, `<<`, `>>`, `<=`,
   `>=`, `<>`, `->`, `+=`, `-=`, `*=`, `/=`, `\=`, `^=`, `&=`, `..`, `...`).
4. ✅ Classify all single-char operators with `OperatorKind`.
5. ✅ Binary vs unary context detection (`m_canBeUnary` flag).
6. ✅ `=` always `Assign`.
7. ✅ Tests for compound operators as single tokens.
8. ✅ Tests for binary vs unary classification.

### Phase 2: Format tree and builder ✅

Variant-based tree: `Node = variant<BlankLineNode, StatementNode, unique_ptr<BlockNode>>`.
Branches are child BlockNodes with opener but no closer. Stack-based TreeBuilder
with append/statement/openBlock/openBranch/closeBlock/blankLine/finish.
14 TreeBuilder tests passing.

9.  ✅ `FormatTree.hpp` — `BlankLineNode`, `StatementNode`, `BlockNode`, `Node`
    variant, `ProgramTree`, `visitNode` helper.
10. ✅ `TreeBuilder.hpp/.cpp` — stack-based builder with branch support.
    `openBranch()` auto-closes previous branch. `closeBlock()` auto-closes
    branch before closing parent. `finish()` auto-closes unclosed blocks.
11. ✅ 14 unit tests: statements, blocks, nested blocks, If/Else branches,
    Select/Case branches, PP blocks, blank lines, malformed input
    (unmatched closers, unclosed blocks/branches), edge cases.

### Phase 3: Scanner — token stream to tree ✅

Scanner iterates lexer tokens and drives TreeBuilder. Processes one physical
line at a time: collects tokens (skipping whitespace), splits on colons,
handles `_` continuation, then dispatches based on first structural keyword.
23 scanner tests passing (full pipeline: source → Lexer → Scanner → tree).

12. ✅ `Scanner.hpp/.cpp` — static `scan()` entry point, `processLine()` for
    line collection, `dispatch()` for keyword/PP routing.
13. ✅ Line collection with whitespace skip, colon splitting (`:`→ new segment),
    and `_` continuation (lookahead to verify standalone `_` at line end).
14. ✅ Keyword dispatch: block openers → openBlock, closers → closeBlock,
    mid-block (Else/ElseIf/Case) → openBranch, Declare → statement,
    If/Then (last significant keyword check), Type/As (second keyword check).
15. ✅ PP dispatch: openers → openBlock, closers → closeBlock, mid-block
    (#else/#elseif) → openBranch, other (#include/#define) → statement.
16. ✅ Blank line detection (consecutive newlines → blankLine).
17. ✅ Malformed input: unmatched closers → statement, unclosed blocks →
    auto-closed by TreeBuilder.finish().
18. ✅ 23 tests: statements, Sub/For/nested blocks, If/Else/ElseIf, Select/Case,
    Type/Type As, Declare, PP blocks, colon splitting, blank lines, malformed.

### Phase 4a: Renderer — indentation ✅

Tree structure makes indentation trivial — no explicit PP indent stack needed.
Branches detected by first token's `KeywordKind` and emitted at parent indent.
22 renderer tests passing (full pipeline: source → Lexer → Scanner → Renderer).

19. ✅ `Renderer.hpp/.cpp` — ~90 lines. `render()` returns formatted string.
    `renderNodes` dispatches via `std::visit` + `overloaded{}`.
20. ✅ Tree walk: `StatementNode` at current indent. `BlockNode` opener at
    indent N, body at N+1, closer at N.
21. ✅ Branch detection: `isBranch()` checks first token for Else/ElseIf/Case/
    PP mid-block. Branches render at `indent-1` (parent level).
22. ✅ PP indent implicit in tree nesting — no save/restore stack needed.
    PP blocks are just BlockNodes inside code blocks, natural +1 indent.
    PP branches (#else) detected same as code branches.
23. ✅ 22 tests: statements, Sub/nested blocks, If/Else/ElseIf, Select/Case,
    For/Do/While, Type/Scope, PP blocks (ifdef/else/endif, nested, inside
    code blocks, code indent reset at #else), colon splitting, strip
    existing indent, blank lines preserved.

### Phase 4b: Renderer — spacing ✅

`needsSpaceBefore()` checks OperatorKind pairs. No space inside parens/brackets,
before commas, after unary ops, around dot/arrow. Parens after operators get
space (grouping), after identifiers/keywords don't (call). 17 new spacing tests.

24. ✅ Spacing rules: binary ops (space both sides), unary (no space after),
    parens/brackets (no space inside, smart grouping vs call detection),
    comma (space after only), braces (space inside), dot/arrow (no space),
    keywords/identifiers (single space), semicolon (space both sides).
25. ✅ 17 spacing tests: binary arithmetic, compound assign, comparison,
    shifts, unary negate/deref/addressof, function calls, multi-arg calls,
    array index, nested parens, braces, dot/arrow access, keywords, semicolon.

### Phase 4c: Renderer — blank lines and anchored hash ✅

Blank line collapse, enforcement between definitions, and anchored hash PP
mode. `isDefinition()` checks opener for Sub/Function/Type/Enum/Union/Namespace.
8 new tests (47 renderer tests total).

26. ✅ Blank lines: `BlankLineNode` → newline. Multiple consecutive collapsed
    to 1. Automatic blank line inserted between consecutive definition blocks
    (Sub/Function/Type/Enum/Union/Namespace). Not inserted between non-definition
    blocks (For/While/Do).
27. ✅ Anchored hash: `renderAnchoredPP()` emits `#` at column 0, then
    `indent * tabSize - 1` spaces, then directive text. At indent 0, no space.
28. ✅ 8 new tests: multiple blank line collapse, blank line between defs,
    blank line between function and type, no blank line between non-defs,
    existing blank line not duplicated, anchored hash simple/nested/inside code.

### Phase 5: Integration ✅

`Formatter` class: public API wrapping Scanner → TreeBuilder → Renderer.
All tests pass through the public API. `FormatDialog` wired up. Old
`ReindentTransform` removed.

29. ✅ `Formatter.hpp/.cpp` — `format(tokens) -> string`. Wires
    Scanner::scan → Renderer::render in a single call.
30. ✅ `FormatRendererTests` updated to use `Formatter` as the entry point.
31. ✅ `ReindentTests` ported: all 45 old tests pass with new Formatter.
    Colon tests updated (split into lines instead of preserving one-line form).
    `Function = value` property setter detected via `hasAssignAfterKeyword()`.
32. ✅ `FormatDialog` wired to use `Formatter` for reindent. CaseTransform
    applied to tokens first, then Formatter produces formatted string.
    PlainText path uses string directly; HTML path re-tokenizes.
33. ✅ Old `ReindentTransform.hpp/.cpp` removed. No remaining references.

### Phase 6: Polish

34. Handle edge case: `:` as bitfield separator inside Type blocks (defer —
    treat as statement separator for now, revisit later).
35. ✅ `Function = value` property setter — handled by `hasAssignAfterKeyword()`.
36. Verify no token loss: add a test that round-trips arbitrary input and
    confirms all non-whitespace tokens are preserved.
37. Performance: ensure the formatter handles large files without excessive
    allocation.
