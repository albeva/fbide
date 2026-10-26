# Spec Delta

## Purpose

Defines the `fbide format` command-line formatter: how it reads and decodes its input, which options it accepts, how it writes its output, and how it reports errors.

## ADDED Requirements

### Requirement: Input encoding is detected like the editor
Without `--encoding`, `fbide format` SHALL decode its input file with the same detection the editor uses when opening a file: a BOM wins; otherwise valid UTF-8 is UTF-8; otherwise the configured `editor.encoding` default applies, and Latin-1 is the last resort.

#### Scenario: Latin-1 file formats successfully
- **WHEN** `fbide format --reindent --reformat tests/data/flappyfb.bas` is run on the Latin-1 sample
- **THEN** it exits with code 0, the output is the formatted program, and the accented comment `Grégori Macário` survives

#### Scenario: UTF-8 file with BOM
- **WHEN** the input is UTF-8 with a BOM
- **THEN** it is decoded as UTF-8 and the BOM is not treated as source text

#### Scenario: Plain UTF-8 file
- **WHEN** the input is valid UTF-8 without a BOM
- **THEN** it is decoded as UTF-8, as before this change

### Requirement: `--encoding` forces the input encoding
`fbide format --encoding <name>` SHALL decode the input in `<name>` and skip detection. `<name>` is one of the editor's encoding names (`UTF-8`, `UTF-8-BOM`, `UTF-16-LE`, `UTF-16-BE`, `Windows-1252`, `Windows-1250`, `Windows-1251`, `CP437`, `CP850`, `ISO-8859-1`, `System`), matched case-insensitively.

#### Scenario: Forced encoding overrides detection
- **WHEN** a file containing the byte `0x82` is formatted with `--encoding CP437`
- **THEN** that byte is decoded as `é` (CP437), not as a Latin-1 control character

#### Scenario: Case-insensitive name
- **WHEN** `--encoding windows-1252` is given
- **THEN** it is accepted as `Windows-1252`

#### Scenario: Unknown encoding name
- **WHEN** `--encoding KOI8-R` is given
- **THEN** the command writes an error that names the valid encodings to stderr, writes no output, and exits non-zero

#### Scenario: Missing value
- **WHEN** `--encoding` is the last argument
- **THEN** the command reports that `--encoding` requires a name and exits non-zero

### Requirement: Code output keeps the input encoding
Plain-code output, to stdout or to the `-o` file, SHALL be encoded in the input's encoding (detected or forced), including its BOM when the encoding has one. If the formatted text can't be represented in that encoding, the command SHALL report an error, write nothing, and exit non-zero.

#### Scenario: Latin-1 round trip
- **WHEN** a Latin-1 file is formatted with `-o out.bas`
- **THEN** `out.bas` is Latin-1 (`é` is the single byte `0xE9`), not UTF-8

#### Scenario: BOM preserved
- **WHEN** a UTF-8-BOM file is formatted to stdout
- **THEN** the output starts with the UTF-8 BOM

### Requirement: HTML output is UTF-8
With `--html`, output SHALL be UTF-8, whatever the input encoding, matching the document's declared `<meta charset="utf-8">`.

#### Scenario: HTML from a Latin-1 file
- **WHEN** a Latin-1 file is formatted with `--html`
- **THEN** the HTML is valid UTF-8 and shows `é` correctly

### Requirement: `--encoding` is ignored outside `format`
When the first argument is not `format`, `--encoding <name>` SHALL be accepted and ignored together with its value, instead of failing as an unknown option.

#### Scenario: Normal launch with --encoding
- **WHEN** FBIde is started as `fbide --encoding CP437 file.bas`
- **THEN** it starts normally and opens `file.bas`; `CP437` is not treated as a file to open

### Requirement: Help documents `--encoding`
`fbide --help` SHALL list `--encoding <name>` under the format options, with the accepted names.

#### Scenario: Help text
- **WHEN** `fbide --help` is run
- **THEN** the format options include `--encoding <name>` and the list of names
