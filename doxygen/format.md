# Format {#format}

The formatter rewrites a FreeBASIC source buffer to canonical layout:
indented blocks, normalised inter-token spacing, controlled blank
lines, and (optionally) anchored preprocessor directives. It is
exposed through the Format dialog (`View → Format`) and shares its
front-half — lexer + tree — with the analyses pipeline.

This page focuses on the rendering side. For the lexer details and
how the same tree feeds the symbol browser, see @ref analyses.

## Pipeline

```
   wxString source
        │
        ▼
FBSciLexer (Lexilla styling)
        │  style runs
        ▼
StyleLexer
        │  vector<lexer::Token> with KeywordKind / OperatorKind / line
        ▼
ReFormatter
        │  segments + dispatch
        ▼
TreeBuilder
        │  reformat::ProgramTree
        ▼
Renderer / HtmlRenderer / CaseTransform
        │  vector<lexer::Token>
        ▼
PlainTextRenderer
        │  wxString
        ▼
Editor / clipboard / dialog preview
```

`ReFormatter::buildTree` is exposed for testing the scan stage in
isolation. `Renderer::render(tree)` re-emits a token stream;
`PlainTextRenderer` then concatenates token text into a final string.

## Tree shape

`reformat::ProgramTree` (`FormatTree.hpp`) is a flat root node holding
`Node`s, where each `Node` is one of:

| Variant                         | Meaning                                                 |
|---------------------------------|---------------------------------------------------------|
| `BlankLineNode`                 | One preserved blank line in the source.                 |
| `StatementNode`                 | A flat sequence of tokens for one logical statement.    |
| `VerbatimNode`                  | A `' format off` region — passes through untouched.     |
| `unique_ptr<BlockNode>`         | A nested block (`Sub`/`If`/`#if`/...).                  |

`BlockNode` has an optional opener statement, a body of child nodes,
and an optional closer. Branches (`Else`, `Case`, `#else`) are child
blocks with an opener but no closer — that's how multi-arm constructs
land naturally in the tree.

## Lean mode

`FormatOptions::lean = true` builds a tree suitable for non-rendering
consumers (the sub/function browser, future analyses):

- Whitespace, Comment, and CommentBlock tokens are dropped.
- Runs of Newline tokens collapse into a single separator.
- Tokens *inside* a `' format off` verbatim region still pass through
  untouched, so even lean consumers see the literal text.

`SymbolTable` walks a lean tree directly. The renderer never reads
lean trees — its rendering rules need the layout tokens.

## Verbatim regions

`' format off` and `' format on` markers are recognised by the
`VerbatimAnnotator` pass and emitted as `VerbatimNode`s in the tree.
The renderer copies those tokens through with their original text
and whitespace and skips:

- Indent prefixing.
- Inter-token space normalisation.
- Blank-line collapsing.

The two markers themselves stay in the buffer (so the user can see the
boundary) and are part of the verbatim node.

## Anchored PP rendering

`FormatOptions::anchoredPP = true` flips `#`-at-column-0 mode: every
preprocessor directive starts in column 0 even when nested inside an
indented block. The merged Preprocessor token covers the whole line,
so the renderer (`renderAnchoredPP`) rebuilds the token text by
splitting `# whitespace directive` and padding the inner whitespace
to keep the directive word at the indent level it would have had
otherwise. The result reads as `#` at the margin with the directive
floating where the surrounding code sits.

## Renderer responsibilities

`Renderer` (`format/transformers/reformat/Renderer.hpp`) owns:

- **Indentation** — block depth × `tabSize` spaces emitted as
  Whitespace tokens at the start of each statement line.
- **Inter-token spacing** — `needsSpaceBefore(prev, curr)` decides
  whether to emit a space between two tokens (e.g. binary `+` vs
  unary `-`, function call paren vs grouping paren).
- **Line continuations** — `_` at end-of-line is preserved when the
  source had it; the renderer doesn't introduce new continuations.
- **Blank-line policy** — a single blank between top-level
  definitions (Sub / Function / Type), no leading or trailing
  blanks. Tracked via `m_lastWasBlock` / `m_lastWasBlankLine`.
- **Trailing newline** — guaranteed exactly one at the end of the
  output.

## Renderer variants

| Renderer            | Output       | Use                                           |
|---------------------|--------------|-----------------------------------------------|
| `Renderer`          | tokens       | Standard reformat back to source.             |
| `HtmlRenderer`      | tokens + spans | Coloured HTML for the Format dialog preview pane and "Copy as HTML". |
| `CaseTransform`     | tokens       | Pure keyword-case rewrite (no layout changes). Composable with `Renderer`. |

`PlainTextRenderer` (`format/renderers/PlainTextRenderer.hpp`) is the
final stringifier — given any token stream, it concatenates token
text into a `wxString`.

## Recipe: add a new format option

1. Add the field to `FormatOptions` in `FormatTree.hpp`.
2. Add the UI control to `FormatDialog`; wire its value through to
   the options struct passed into `ReFormatter` / `Renderer`.
3. Implement the behaviour in `Renderer` (or upstream in `ReFormatter`
   if it changes the tree shape).
4. Add unit tests under `tests/format/`.

## Cross-links

- @ref analyses — same lexer + tree feeds the symbol browser; lean
  mode is what analyses uses.
- @ref editor — `View → Format` opens the dialog with a preview
  editor pair.
- @ref settings — defaults for `format.tabSize`, `format.anchoredPP`,
  etc. live in the Settings dialog.
- @ref theming — `HtmlRenderer` reads the active theme so exported
  HTML matches the editor.
