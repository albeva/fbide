# Tasks

TDD throughout: write the tests, run them and confirm they fail for the stated reason, then implement. Build: `cmake --build build/claude/debug` (configured with `-DBUILD_TESTING=ON`). Unit tests: `./build/claude/debug/tests/tests --gtest_filter='<Suite>.*'`. On macOS never run `ui-tests` (CTest label `gui`). CLI binary: `bin/fbide.app/Contents/MacOS/fbide` on macOS, `bin/fbide.exe` on Windows, `bin/fbide` on Linux.

## 1. Encoding helpers (design D1)

- [x] 1.1 Add `tests/unit/FormatIOTests.cpp` (registered in `tests/CMakeLists.txt`) with failing cases:
  - `parseEncodingName` accepts every editor name exactly and case-insensitively (`windows-1252`, `utf-8-bom`), and rejects `KOI8-R` and `""`.
  - `encodingNames()` contains all 11 names.
  - `encodeWithBom` encodes `"é"` as `{0xE9}` for `ISO-8859-1`, `{0x82}` for `CP437`, and `{0xEF,0xBB,0xBF,0xC3,0xA9}` for `UTF-8-BOM`, with a UTF-16-LE BOM for `UTF-16-LE`. It returns `nullopt` for a character the encoding lacks (e.g. `中` in `Windows-1252`).

  Verify the test file fails to build before the helpers exist.
- [x] 1.2 Add `src/lib/format/FormatIO.{hpp,cpp}` (registered in `src/lib/CMakeLists.txt`) implementing D1 on top of `TextEncoding`. Verify `FormatIOTests` passes.

## 2. Raw console output (design D3)

- [x] 2.1 Add `ConsoleOutput::writeRaw(std::string_view)` that forwards to the existing private `writeBytes`. Verify the build succeeds and existing tests pass. It is exercised end to end in group 4.

## 3. Decode input and encode output in `fbide format` (design D2, D4)

- [x] 3.1 Add fixture files under `tests/data/encoding/`:
  - `cp437.bas`: a short program whose comment holds byte `0x82`
  - `utf8-bom.bas`: BOM plus `Print "é"`
  - `utf16le.bas`: BOM plus the same program
  - `flappyfb.bas` (Latin-1) is reused as is

  Before changing code, run the CLI on each (`format --reindent --reformat`) and record that the non-UTF-8 ones fail (`formatting failed`, exit 1). That is the failing baseline.
- [x] 3.2 Add `std::optional<TextEncoding> encoding` to `FormatCommand::Options`. In `run`, load through `DocumentIO::load` (configured `editor.encoding` default) or `DocumentIO::loadWithEncoding` when forced, then lex `utf8_str()` of the decoded text. Verify the existing `FormatCheck*` and `ReFormatterTests` still pass.
- [x] 3.3 Encode code output with `encodeWithBom(…, loadedEncoding)` and write it with `writeRaw` or the `-o` file. On encode failure, print `fbide: cannot encode output as <name>: <file>` and exit 1 with no output. Keep `--html` output UTF-8 (D4). Verify:
  - `flappyfb.bas` formats with exit 0, and the comment bytes stay `0xE9` / `0xE1`
  - `utf8-bom.bas` output starts with `EF BB BF`
  - `utf16le.bas` output starts with `FF FE` and is valid UTF-16-LE
  - `--html` on `flappyfb.bas` is valid UTF-8 containing `é`

## 4. `--encoding` option (design D5)

- [x] 4.1 In the `format` branch of `App::parseCli`, parse `--encoding <name>` with `parseEncodingName`. A missing value or unknown name reports an error (the unknown-name error lists `encodingNames()`) and fails parsing. Pass the result through `CliOptions::formatEncoding` into `FormatCommand::Options`. Verify:
  - `format --encoding CP437 tests/data/encoding/cp437.bas` decodes `0x82` as `é`: with `--html`, the output contains `é`.
  - `--encoding koi8-r` and a trailing `--encoding` each exit non-zero with the right message.
  - `--encoding windows-1252` is accepted.
- [x] 4.2 In the normal-launch argument loop, make `--encoding` consume its value and continue (log it with `--verbose`). Verify that `fbide --encoding CP437 --version` prints the version and exits 0, where it currently fails with `unknown option`.
- [x] 4.3 Add `--encoding <name>` to the `--help` format options, with the name list and the note that `System` is the platform default code page. Verify `fbide --help` shows it.

## 5. Changelog and integration

- [x] 5.1 Add an entry under `# Changes since 0.5.1` in `change-log.md`: `fbide format` detects the file encoding, writes output in the same encoding, and supports `--encoding`. Verify the entry is present.
- [x] 5.2 Run a full build and all unit tests: `ctest --test-dir build/claude/debug -LE gui --output-on-failure` (on Windows, run `ui-tests` too). Verify all pass, and re-run the group 3–4 CLI checks against the final binary.

## Workflow follow-up

- Archive with `/opsx:archive` after review.
