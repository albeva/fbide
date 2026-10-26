# block-structure Specification

## Purpose

Defines how FreeBASIC source is organised into nested blocks (procedures, loops, conditionals, `#if` regions). The formatter, the Sub/Function browser and intellisense rely on this structure.

## Requirements

### Requirement: Blocks closed in every preprocessor branch are closed after the conditional
When a block opened before a `#if` is closed by the same number of closers in every branch of the conditional, and the conditional has an `#else`, the block SHALL be treated as closed at `#endif`. Statements and procedures after `#endif` SHALL belong to the enclosing scope.

#### Scenario: Procedure after a split Do loop stays top-level
- **WHEN** the source has `Sub A` containing `Do … #if X … Loop Until p … #else … Loop Until q … #endif`, then `End Sub`, followed by `Sub B … End Sub`
- **THEN** `A` and `B` are both top-level procedures, and `B` is not nested inside `A`

#### Scenario: Function list after a split closer
- **WHEN** the Sub/Function browser lists the source above
- **THEN** it lists both `A` and `B` as top-level procedures

#### Scenario: Issue #136 delayHR sample
- **WHEN** the `delayHR` procedure from issue #136 is followed by another `Sub`
- **THEN** both procedures are top-level, and `End Sub` closes `delayHR`

### Requirement: Uneven preprocessor branch closers keep the enclosing block open
When the branches of a conditional close different numbers of outer blocks, or there is no `#else`, the closers SHALL stay statements of their branch, and the outer block SHALL remain open after `#endif`.

#### Scenario: Only the #if branch closes the loop
- **WHEN** `Do` is followed by `#if X` / `Loop` / `#endif` / `Loop`
- **THEN** the final `Loop` closes the `Do`, and the `Loop` inside `#if` is a statement of that branch

### Requirement: Unmatched closers do not end a preprocessor branch
A block closer inside an `#else`, `#elseif`, `#elseifdef` or `#elseifndef` branch that has no matching opener in that branch SHALL NOT end the branch. Following lines up to the next branch directive or `#endif` SHALL stay in that branch.

#### Scenario: Statement after a closer in #else stays in the branch
- **WHEN** an `#else` branch has `Loop Until q` followed by `x = 1`, before `#endif`
- **THEN** `x = 1` belongs to the `#else` branch
