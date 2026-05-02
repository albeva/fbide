# Documents {#documents}

A *document* is a single open file (or untitled buffer) the user can
edit, save, and close. Every document carries enough state — path,
encoding, EOL mode, modtime, latest symbol table — to round-trip
between disk and the editor without leaning on wxSTC's built-in I/O.

## The model

`fbide::Document` (`src/lib/document/Document.hpp`) is a thin owning
struct:

| Field           | Purpose                                                   |
|-----------------|-----------------------------------------------------------|
| `m_filePath`    | Absolute path. Empty until first save (`isNew()`).        |
| `m_type`        | `DocumentType` (FreeBASIC, HTML, Properties, Text).       |
| `m_editor`      | `Unowned<Editor>` — wx-parented to the notebook page.     |
| `m_modTime`     | Last on-disk mtime; backs `checkExternalChange()`.        |
| `m_encoding`    | Bytes-to-text codec used on save.                         |
| `m_eolMode`     | Line-ending convention applied on save.                   |
| `m_metaModified`| Encoding/EOL change flag — OR'd into `isModified()`.      |
| `m_symbolTable` | `shared_ptr<const SymbolTable>` — latest analyses result. |

The `Editor` widget lives in the AUI notebook page, so the lifetime is
"document is alive while the tab exists". `Document` itself is owned by
`DocumentManager`'s `vector<unique_ptr<Document>>`.

## DocumentManager

`fbide::DocumentManager` (`src/lib/document/DocumentManager.hpp`) is the
single owner of every open document plus the cross-cutting concerns
that span them. It also serves as the `wxEvtHandler` for the find /
replace dialog and the intellisense result event.

### Open / save / close

| Method                                | Notes                                                                 |
|---------------------------------------|-----------------------------------------------------------------------|
| `newFile(type)`                       | Untitled buffer, immediate active.                                    |
| `openFile(path)`                      | Idempotent — returns existing tab if path already open.               |
| `openFile()`                          | Shows wxFileDialog, opens selection.                                  |
| `openInclude(origin, path)`           | Resolves `#include` against origin's dir, then `fbc/inc`, then cwd.   |
| `saveFile(doc)`                       | Falls back to `saveFileAs` when untitled.                             |
| `saveFileAs(doc)`                     | Prompts for path; updates type from extension.                        |
| `saveAllFiles()`                      | Returns false on user cancel.                                         |
| `closeFile(doc)`                      | Prompts on dirty buffer; cancels intellisense before erase.           |
| `closeAllFiles()` / `closeOtherFiles` | Sequential close with cancel propagation.                             |
| `reloadFromDisk(doc)`                 | Re-runs the load pipeline; preserves user's existing encoding/EOL.    |
| `reloadWithEncoding(doc, encoding)`   | Forces a codec, bypasses detection.                                   |

`prepareToQuit()` walks the dirty list once and folds save-or-discard
prompts into a single confirmation; the frame's close handler defers to
it before letting the app exit.

### DocumentIO

`fbide::DocumentIO` (`src/lib/document/DocumentIO.hpp`) is a static
helper that replaces wxSTC's built-in `LoadFile` / `SaveFile`:

- `load` — autodetect encoding via `EncodingDetector`; strip BOM;
  detect EOL; never fails on decode (fallback ISO-8859-1).
- `loadWithEncoding` — bypass detection, force a codec.
- `save` — convert EOLs, encode, write. Distinguishes `IOError`
  (write failure) from `EncodingError` (codec rejected text — nothing
  is written, document stays dirty so the user can switch encoding).

`Document::setEncoding` / `setEolMode` set `m_metaModified` so
metadata-only edits round-trip through "modified" → "saved" the same
way text edits do.

## External change detection

`Document::checkExternalChange()` compares the current on-disk mtime
against `m_modTime`. `DocumentManager::saveFile` runs the check before
overwriting and prompts the user; the same primitive backs the
"Reload from Disk" command. `updateModTime` is called after every
successful load and save so the next check measures from the most
recent known good state.

## FileSession

`fbide::FileSession` (`src/lib/document/FileSession.hpp`) reads and
writes `.fbs` workspace snapshots: which files are open, where the
caret sits, encoding/EOL choices.

Format version is kept at the top — current is **v3**, INI-based:

```ini
[session]
version=3
selectedTab=0

[file_000]
path=C:/path/to/foo.bas
scroll=10
cursor=250
encoding=UTF-8
eolMode=LF
```

Legacy formats (v0.1 unversioned text, v0.2 with an XML-ish header)
still load via `loadLegacy` so existing user sessions don't break, but
every save writes v3. Bump `FileSession::Version` when the schema
changes and add a new `loadVN` branch keyed off the detected version.

## Recipe: add a new document type

1. Add a value to `enum class DocumentType` (`DocumentType.hpp`) and a
   `displayName` / `extensions` row to its dispatch table.
2. Teach `Editor::applyTheme` how to dispatch the lexer for the new
   type — see @ref theming.
3. If the new type has its own analyses, hook it into the intellisense
   pipeline; otherwise leave `m_symbolTable` empty.
4. Update the open-file filter and the save-as filter to advertise
   the new extension.

## Cross-links

- @ref editor — `Editor` widget, themes, on-type transforms.
- @ref analyses — symbol tables and how `setSymbolTable` is delivered.
- @ref commands — file menu wiring + `syncEditCommands`.
- @ref settings — config-derived encoding / EOL defaults.
