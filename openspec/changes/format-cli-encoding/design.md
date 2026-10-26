# Design

## Context

See proposal.md (Why). Current flow in `FormatCommand::run`: `wxFFile` reads raw bytes, the bytes go straight to `MemoryDocument`, then lexing and transforms. The result is checked by `isSafeFormatResult` (which rejects invalid UTF-8) and rendered. `emit` writes `ToStdString(wxConvUTF8)` to stdout through `ConsoleOutput::write`, or to the `-o` file.

The pieces to reuse already exist:
- `DocumentIO::load(path, defaultEncoding, defaultEol)`: the editor's detection pipeline (BOM, UTF-8, fallback, Latin-1). It returns the text, the encoding and the EOL.
- `DocumentIO::loadWithEncoding(path, encoding, defaultEol)`: forced decode with matching-BOM strip.
- `TextEncoding::parse(name)` (exact match on the config key), `encode(wxString) -> optional<wxCharBuffer>`, and `bomBytes()`.
- The editor's default encoding is `editor.encoding` in config, read by `DocumentManager::defaultEncoding()` (UTF-8 if unset).
- `ConsoleOutput` has a private `writeBytes(std::string, toStderr)`, which uses `WriteFile` on Windows and `std::cout` elsewhere.

`App` and `Context` can't be constructed in unit tests, so neither `parseCli` nor `FormatCommand` is unit-testable directly.

## Goals / Non-Goals

**Goals:**
- One decode path shared with the editor, so the CLI and the GUI agree on a file's encoding.
- Testable pieces without a running `App`.

**Non-Goals:**
- EOL conversion. The text keeps its source line endings, which survive through the tokens as they do today.
- Encoding detection heuristics beyond what `DocumentIO` already does.

## Decisions

### D1: Small, pure I/O helpers in the format module
Add `src/lib/format/FormatIO.{hpp,cpp}` with three functions:

```
/// Case-insensitive TextEncoding name lookup ("windows-1252" -> Windows_1252).
[[nodiscard]] auto parseEncodingName(std::string_view name) -> std::optional<TextEncoding>;

/// Comma-separated list of accepted names, for help and error text.
[[nodiscard]] auto encodingNames() -> std::string;

/// Encode `text` in `encoding`, BOM first when the encoding has one. Nullopt if
/// some character can't be represented.
[[nodiscard]] auto encodeWithBom(const wxString& text, TextEncoding encoding) -> std::optional<std::string>;
```

Loading needs no new helper: `FormatCommand` calls `DocumentIO::load` or `DocumentIO::loadWithEncoding` directly. All three helpers are unit-testable.
- *Alternative:* put the case-insensitive lookup in `TextEncoding::parse`. Rejected: config keys are exact today, and loosening them changes how config is parsed elsewhere.

### D2: `FormatCommand` decodes through `DocumentIO`
`FormatCommand::Options` gains `std::optional<TextEncoding> encoding`. `run`:
1. Calls `DocumentIO::load(path, configuredDefault, EolMode::LF)`, or `loadWithEncoding(path, *encoding, EolMode::LF)` when forced. The EOL argument only matters for detection metadata, which is unused. A `nullopt` result is an I/O error: keep today's "cannot open" message.
2. Turns the decoded `wxString` into UTF-8 (`utf8_str()`) and lexes that, so the lexer, transforms and `isSafeFormatResult` keep working on UTF-8 as before.
3. Renders and checks the result as today.
4. For code output, calls `encodeWithBom(FromUTF8(result), loadedEncoding)`. On `nullopt`, prints `fbide: cannot encode output as <name>: <file>` and returns `EXIT_FAILURE` without writing. HTML output stays UTF-8 (D4).
5. Writes the bytes with `emitBytes`.

The configured default comes from `editor.encoding` via `TextEncoding::parse`, with UTF-8 as the fallback. That's the same lookup `DocumentManager::defaultEncoding()` uses. `FormatCommand` can read the config directly (it has `Context`), or the lookup can be moved to a shared free function if duplicating it feels wrong (decide during implementation; either is fine).

### D3: Raw byte output
- Add `ConsoleOutput::writeRaw(std::string_view bytes)`, which forwards to the existing `writeBytes`.
- `FormatCommand::emit` becomes `emitBytes(const std::string&)`, used for both stdout and `-o`. The file path already writes bytes; only the source of the bytes changes.

### D4: HTML stays UTF-8
`HtmlRenderer::decorate` emits `<meta charset="utf-8">`, so HTML output is UTF-8 encoded whatever the input encoding was. Re-encoding it would contradict the declared charset.

### D5: CLI parsing
- In the `format` branch of `App::parseCli`, add `--encoding <name>`: a missing value is an error ("requires an encoding name"). An unknown name is an error listing `encodingNames()`, and `parseFailed` sets a non-zero exit (the existing path). Store the result in `CliOptions::formatEncoding` (`std::optional<TextEncoding>`) and pass it to `FormatCommand::Options`.
- In the normal-launch loop, `--encoding` consumes its next argument and continues (logged only with `--verbose`). If it is the last argument, it's simply skipped.
- Help text: add `--encoding <name>` with the name list and a note that `System` is the platform's default code page.

### D6: TDD order
Unit tests for `FormatIO` first, then the `FormatCommand` change, verified with the built binary on fixture files. The CLI tests are scripted commands recorded in tasks, because `App` isn't unit-testable.

## Risks / Trade-offs

- [`System` differs between machines] → It's documented in the help text. Tests don't use it.
- [Detection picks the wrong 8-bit code page] (e.g. a CP437 file detected as the fallback) → That's what `--encoding` is for. Detection is the editor's, so the CLI is no worse than opening the file in the editor.
- [The PUA mapping for invalid UTF-8 behind a UTF-8 BOM] → `DocumentIO::load` decodes those lossily, and `encode` reverses it byte for byte. `utf8_str()` of PUA characters is valid UTF-8, so `isSafeFormatResult` accepts it, and the output re-encodes to the original bytes.
- [Output that can't be encoded] → This only happens if formatting introduced characters the input encoding lacks. The formatter only touches whitespace and ASCII case, so it shouldn't occur, but it is handled anyway (error, no output).
