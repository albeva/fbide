# code-formatter Specification

## Purpose

Defines what the Format command guarantees about its output: it may change layout (indentation, spacing, line breaks) but never the meaning or the non-whitespace text of the source.

## Requirements

### Requirement: Multi-byte characters survive formatting
Formatting SHALL output every multi-byte UTF-8 character of the source byte for byte. It SHALL NOT insert whitespace inside a character or between adjacent non-ASCII characters, wherever they appear (code, identifiers, preprocessor bodies, strings, comments).

#### Scenario: Non-ASCII identifier
- **WHEN** `Dim 变量 As Integer` is reformatted
- **THEN** the output is `Dim 变量 As Integer`

#### Scenario: Non-ASCII text in a define body
- **WHEN** `#define X 中` is reformatted
- **THEN** the output is `#define X 中`

#### Scenario: Full-width punctuation in code
- **WHEN** `x = 1 ： y = 2` is reformatted
- **THEN** the output contains `：` unchanged, and the result is valid UTF-8

### Requirement: No new spaces next to unrecognised text
Formatting SHALL NOT add a space between a token it cannot classify (such as non-ASCII text) and an adjacent token when the source had no whitespace between them. Where the source had whitespace there, the output SHALL have a single space.

#### Scenario: Glued ASCII and non-ASCII
- **WHEN** `x变量 = 1` is reformatted
- **THEN** the output keeps `x变量` together

### Requirement: Glued binary operators stay glued
Formatting SHALL NOT insert a space between two adjacent binary-operator tokens that are glued in the source, unless the second is a unary, bracket or separator operator. Spacing around the glued run follows the normal rules.

#### Scenario: Concat-assign followed by greater-than
- **WHEN** `a &=> "-"` is reformatted
- **THEN** the output is `a &=> "-"`

#### Scenario: Unary minus after an operator still gets normal spacing
- **WHEN** `a=-1` and `a<=-1` are reformatted
- **THEN** the outputs are `a = -1` and `a <= -1`

### Requirement: A failed format never replaces the source
Before a formatted result replaces the source, it SHALL be checked. It fails when it is empty for non-empty input, or when its non-whitespace text differs from the source's, ignoring ASCII letter case and `:` separators removed by statement splitting. On failure the source SHALL stay unchanged and the user SHALL be told formatting failed.

#### Scenario: Reformat command on a failing document
- **WHEN** the Reformat command produces a result that fails the check
- **THEN** the editor text is unchanged, no undo step is added, and a "Formatting failed" message is shown

#### Scenario: Format dialog with a failing result
- **WHEN** the Format dialog's result fails the check
- **THEN** Apply does not change the editor, and the dialog tells the user formatting failed

#### Scenario: Command-line format with a failing result
- **WHEN** `fbide format` produces a result that fails the check
- **THEN** it writes an error message to stderr, writes no output, and exits with a non-zero code

#### Scenario: Legitimate formatting passes the check
- **WHEN** a document is formatted with every option enabled (re-indent, re-format, align PP, case) and the formatter works correctly
- **THEN** the check passes and the formatted text is applied, including when keyword case changed and `:` separators were split onto new lines

### Requirement: Formatting preserves non-whitespace source text
Formatting SHALL keep every non-whitespace character of the source, in order. Only whitespace, line breaks and keyword case (when case conversion is enabled) may change. This holds inside preprocessor directive bodies too.

#### Scenario: Member access inside a define body
- **WHEN** the source `#Define LEFT1 (sprite1.x)` is formatted
- **THEN** the output is `#Define LEFT1 (sprite1.x)`

#### Scenario: Member access on a call argument inside a define body
- **WHEN** the source `#Define rng float32(myhash.v)` is formatted
- **THEN** the output is `#Define rng float32(myhash.v)`

#### Scenario: Ellipsis inside a define body
- **WHEN** a `#define` body contains `...`, as in `#define V(a, ...) f(a, __VA_ARGS__)`
- **THEN** the `...` appears unchanged in the output

### Requirement: Single-line If keeps all of its statements
Formatting SHALL NOT split a single-line `If … Then <statement> : <statement>` at its colons, because every statement after `Then` belongs to the condition. A block-form `If … Then` (nothing significant after `Then`) SHALL still be split at its colons into a multi-line block.

#### Scenario: Single-line If with two statements
- **WHEN** `If t2 < t1 Then t1 -= 1 : t3 -= 1` is formatted
- **THEN** the output is the same single line, `If t2 < t1 Then t1 -= 1 : t3 -= 1`

#### Scenario: Single-line If with Else and colons
- **WHEN** `If c Then a = 1 : b = 2 Else a = 3 : b = 4` is formatted
- **THEN** the output stays on one line, with each colon spaced as ` : `

#### Scenario: Block If written on one line is still split
- **WHEN** `If x Then : Print x : End If` is formatted
- **THEN** the output is `If x Then` / `    Print x` / `End If` on three lines

### Requirement: Closers inside preprocessor branches are indented with the branch
When a block opened before a `#if` is closed inside every branch of that conditional, formatting SHALL indent each closer as a statement of its branch. Code after `#endif` SHALL be indented as if the block were closed.

#### Scenario: Do loop closed in both branches
- **WHEN** this is formatted:
  ```
  Sub S
  Do
  #if A
  x = 1
  Loop Until x
  #else
  Loop Until y
  #endif
  End Sub
  ```
- **THEN** the output is:
  ```
  Sub S
      Do
          #if A
              x = 1
              Loop Until x
          #else
              Loop Until y
          #endif
  End Sub
  ```

#### Scenario: Code after the conditional returns to the outer level
- **WHEN** a statement follows `#endif` in the scenario above, before `End Sub`
- **THEN** that statement is indented one level (the Sub body), not two
