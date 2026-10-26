# Spec Delta

## Purpose

Defines what the Format command guarantees about its output: it may change layout (indentation, spacing, line breaks) but never the meaning or the non-whitespace text of the source.

## ADDED Requirements

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
