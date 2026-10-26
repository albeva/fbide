# Proposal

## Why

`fbide format` reads the input file as raw bytes and assumes UTF-8. The editor, by contrast, detects each file's encoding when it opens it. Older FreeBASIC sources are often Latin-1, Windows-125x or DOS code pages, and the command line can't format them. Before the `fix-format-data-loss` safety check, they produced empty output with exit code 0. Now they fail with `fbide: formatting failed`. The bundled `tests/data/flappyfb.bas` (Latin-1 with `é`/`á` in a comment) is an example.

## What Changes

- `fbide format` detects the input file's encoding exactly as the editor does when opening a file: BOM first, then a UTF-8 check, then the configured `editor.encoding` default, then Latin-1 as a last resort. It formats the decoded text.
- New option `fbide format --encoding <name>` forces the input encoding and skips detection. The names are the editor's encoding names (`UTF-8`, `UTF-8-BOM`, `UTF-16-LE`, `UTF-16-BE`, `Windows-1252`, `Windows-1250`, `Windows-1251`, `CP437`, `CP850`, `ISO-8859-1`, `System`), matched case-insensitively. An unknown name is an error that lists the valid names, with a non-zero exit code.
- Plain-code output (stdout or `-o <file>`) is written in the same encoding as the input, BOM included, just as the editor writes a file back on save. A Latin-1 file stays Latin-1, and `-o` over the original file doesn't silently convert it.
- HTML output (`--html`) stays UTF-8, because the HTML document declares `<meta charset="utf-8">`.
- If the formatted text can't be represented in the input encoding, the command reports an error, writes nothing, and exits non-zero (same contract as the editor's save).
- `--encoding <name>` given without the `format` command is accepted and ignored, value included, instead of failing as an unknown option.
- The `--help` text documents `--encoding`.

Out of scope:
- Line endings: the formatter keeps the source's line endings as it does today. No EOL conversion option.
- Changing how the editor detects or saves encodings.
- Any GUI change.

Classic-FBIde parity: the classic IDE had no command-line formatter, so parity is unaffected. Legacy formats: no `.fbt`, `.fbl`, `.ini`, `.lng` or `.fbs` change. The existing `editor.encoding` setting is only read.

## Capabilities

### New Capabilities
- `format-cli`: the `fbide format` command-line formatter: how it reads its input (encoding detection or `--encoding`), how it writes its output (encoding, HTML), and its errors and exit codes.

### Modified Capabilities
<!-- none: no specs exist yet in openspec/specs -->

## Impact

- `src/lib/app/App.{hpp,cpp}`: parse `--encoding` for `format`, skip it for other commands, update the help text.
- `src/lib/format/FormatCommand.{hpp,cpp}`: load through `DocumentIO`, encode the output.
- `src/lib/utils/ConsoleOutput.{hpp,cpp}`: a raw-bytes stdout write (the private `writeBytes` already exists).
- Reused unchanged: `DocumentIO::load` / `loadWithEncoding`, `TextEncoding::parse` / `encode` / `bomBytes`.
- Tests: new `tests/unit/FormatCommandTests.cpp` (or extend an existing suite), and CLI argument tests if `parseCli` is reachable from tests.
- Platforms: Windows writes stdout through `WriteFile`, Unix through `std::cout`. Both write raw bytes, so the output encoding behaves the same on all three. `System` means the platform's default code page and differs between machines; the help text says so.
