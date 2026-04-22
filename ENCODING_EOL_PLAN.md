# Encoding & Line-Ending Awareness — Implementation Plan

## Scope

Make the editor aware of text encoding and line-ending style:

- Global defaults in settings (General panel), persisted in INI
  - one in the left column and the other in the right column, both shown as dropdowns
- Encoding-aware load/save (detect on load, preserve/convert on save)
- Status bar indicators (right side) for current document's EOL + encoding
  - This is per document, and updates when focus changes to another document, similar to cursor position.
- Clickable status bar menus to change EOL / encoding for the active document
- New blank documents inherit editor defaults
- Paste: no special handling needed (see analysis)

## Analysis of key design decisions

### Paste handling
Clipboard text arrives as decoded Unicode. wxSTC stores it as UTF-8 in its buffer regardless of file encoding. **No action required on paste.** The document's encoding is applied only at save time. If pasted characters cannot be represented in the target encoding, handle at save boundary (lossy-conversion warning).

### Encoding change on an open document
Changing encoding does **not** mutate buffer contents (buffer is always UTF-8 in wxSTC). It only changes what bytes get written on next save. Two possible meanings:

1. **Re-interpret** — user is saying "the file on disk was actually this encoding, decode it again" → needs reload from disk using new encoding. Useful when auto-detect got it wrong.
2. **Convert on save** — user is saying "write this file using this encoding from now on" → mark dirty, no buffer change.

Offer both via the menu. Top items = "Save with Encoding ..." (convert on save). Separator. Bottom item = "Reload with Encoding ..." (re-decode from disk). Reload requires disk file to exist and refuses if buffer dirty (prompt to discard).

### EOL change on an open document
- `SetEOLMode(newMode)` — affects new insertions
- `ConvertEOLs(newMode)` — rewrites existing buffer content
- Both called together. Mark document dirty.

### Load detection order
1. BOM sniff (reliable)
2. UTF-8 validator (high confidence if valid non-ASCII sequences present; ASCII-only → any superset works, stick with default)
3. Fallback to configured default encoding

### EOL detection on load
Count `\r\n`, lone `\n`, lone `\r` in first ~8KB. Pick winner. Tie or empty → use configured default.

### Supported encodings
- `UTF-8` (no BOM)
- `UTF-8-BOM`
- `UTF-16-LE` (BOM)
- `UTF-16-BE` (BOM)
- `Windows-1252` (Western European)
- `Windows-1250` (Central European)
- `Windows-1251` (Cyrillic)
- `CP437` (DOS original — FB heritage)
- `CP850` (DOS Western)
- `ISO-8859-1`
- `System` (resolves via `wxConvLocal` at runtime)

### Supported EOL modes
- `LF` (Unix)
- `CRLF` (Windows)
- `CR` (classic Mac — rare, include for completeness)

### New document defaults
New (untitled) documents: encoding + EOL from config. Already-open documents: per-doc state, independent of config changes.

### Architecture notes
Module split (inspired by Code::Blocks' separation of concerns — `EncodingDetector` + `LoaderBase` + `cbEditor` — adapted to our scope):

- `lib/editor/TextEncoding.{hpp,cpp}` — `TextEncoding` + `EolMode` enum classes, name/label/config-key helpers, `wxBOM` / `wxFontEncoding` / `wxSTC_EOL_*` mapping, `decode()` / `encode()` codec shims over prebuilt wx converters. **Pure data + codec, no detection, no I/O.**
- `lib/editor/EncodingDetector.{hpp,cpp}` — **new dedicated class** (not folded into `Document`). Static entry points: `detect(bytes, len, fallback) -> DetectionResult`, `detectBom(bytes, len) -> optional<TextEncoding>`, `isValidUtf8(...)`, `isLikelyUtf8(...)`, `detectEol(wxString) -> EolMode`. Composes wx primitives in our detection order. Zero state, easy to unit-test, reusable from any load path (DocumentIO, future project-import tools, tests).
- `lib/editor/DocumentIO.{hpp,cpp}` — load/save helpers. Composes `EncodingDetector` + `TextEncoding::decode/encode` + `wxFile`. Returns explicit `LoadResult` / `SaveResult`. `Editor` and `Document` never touch `wxFile` directly.
- `lib/ui/EncodingMenu.{hpp,cpp}` — menu builder (adapted from Code::Blocks' `EncodingSelector`). `buildEncodingMenu(currentEncoding) -> wxMenu*` with radio items for save-with + separator + reload-with submenu. `buildEolMenu(currentEol) -> wxMenu*` with radio items. Keeps status bar click handler in `UIManager` thin.
- `Document` gains `m_encoding`, `m_eolMode`, `m_hadBom` — **state only**, no detection logic. Setters call the right wxSTC methods (`SetEOLMode`, `ConvertEOLs`) and mark dirty.
- `Editor` stays view-only. EOL mode applied via `SetEOLMode` when a Document is assigned.
- Status bar: 4 fields — welcome | line:col | EOL | encoding. Click handler in `UIManager` uses `EncodingMenu` + dispatches to active `Document` via `DocumentManager`.

### Adapted from Code::Blocks (without expanding scope)
- **Dedicated `EncodingDetector`** class, separate from the document/editor — matches CB's `sdk/encodingdetector.cpp` shape, avoids mixing detection with state
- **Save-with vs reload-with split** in the encoding menu — same UI pattern as CB's `File → Encoding` submenu and Notepad++
- **Stable string keys for encodings** (`"UTF-8"`, `"Windows-1252"` etc.) in config + `fromString` / `toString` helpers — CB pattern, survives enum reordering
- **Menu builder helper class** (`EncodingMenu`) — isolates menu construction, matches CB's `EncodingSelector` role
- **Explicit `hadBom` flag preserved on the document** — CB keeps BOM orthogonal to encoding so round-trips don't lose it. Our plan folds BOM into enum (`UTF-8-BOM` variant) but the `m_hadBom` flag on Document captures the same intent for non-UTF-8-BOM cases (UTF-16 always has BOM → no ambiguity)

### Deliberately NOT adopted from Code::Blocks
- Statistical detection (Mozilla `nsUniversalDetector` port) — adds library + complexity, BOM + UTF-8 validate + default fallback is sufficient for FB sources
- Asynchronous `LoaderBase` — our files are small, sync load is fine
- Per-project force-encoding overrides — not in original fbide feature set
- Region-grouped encoding menu (Western / CJK / etc.) — our supported list is short enough for a flat menu

### wx reuse strategy
Lean on wxWidgets for primitives; wrap only for project-specific enum mapping and to expose detection results that wx hides.

- **BOM detection** — `wxConvAuto::DetectBOM(src, len)` returns `wxBOM` enum (`wxBOM_None`, `wxBOM_UTF8`, `wxBOM_UTF16LE/BE`, `wxBOM_UTF32LE/BE`). Our wrapper = 1 call + `wxBOM → TextEncoding` map.
- **BOM bytes for save** — `wxConvAuto::GetBOMChars(wxBOM, &count)` returns BOM prefix bytes. No need to hardcode.
- **Named codec lookup** — `wxCSConv("windows-1252")` / `wxCSConv("CP437")` / etc. by string. Or `wxCSConv(wxFONTENCODING_CP1252)` via enum. Prebuilt: `wxConvUTF8`, `wxConvLocal`, `wxMBConvUTF16LE`, `wxMBConvUTF16BE`.
- **UTF-8 validator** — `wxConvUTF8.ToWChar(nullptr, 0, src, len) != wxCONV_FAILED`. No hand-rolled state machine needed.
- **Encoding name ↔ enum** — `wxFontMapper::Get()->CharsetToEncoding(name)` + `wxFontMapper::GetEncodingName(enc)` for free round-tripping.
- **wxConvAuto itself** — BOM sniff + fallback in one object. Tempting for load, but detection result not exposed, so we can't populate status bar. Use its pieces (`DetectBOM`, `GetBOMChars`) not the whole thing.

Our own code owns:
- `TextEncoding` enum + `wxBOM → TextEncoding` and `TextEncoding → wxCSConv` maps
- `detectEncoding(bytes, defaultEncoding) -> {TextEncoding, hadBom}` composing the wx primitives in our order (BOM → UTF-8 validate → fallback)
- `detectEol(text) -> EolMode` (no wx equivalent)
- Status bar click + popup menus (UI glue, not in wx)

## Indexed TODOs

Numbered for tracking. Each item is independently shippable where possible.

### Foundation — `TextEncoding` module

1. **Define `TextEncoding` + `EolMode` enum classes** — new `src/lib/editor/TextEncoding.hpp` + `.cpp`. Include: display-string helpers, `fromString` / `toString` (stable config keys, survive enum reordering), `toStcEolMode()`, `toWxBom()` / `fromWxBom()` mapping onto `wxBOM`, `toFontEncoding()` mapping onto `wxFontEncoding` where applicable, list of all supported values for menus (`TextEncoding::all()`, `EolMode::all()`).

2. **Add encoding codec wrappers** — in `TextEncoding.cpp`: `decode(const void* bytes, size_t len, TextEncoding) -> wxString`, `encode(const wxString&, TextEncoding) -> wxScopedCharBuffer`. Thin shims over prebuilt wx converters — `wxConvUTF8` for UTF-8 / UTF-8-BOM, `wxMBConvUTF16LE/BE` for UTF-16, `wxConvLocal` for `System`, `wxCSConv(toFontEncoding(enc))` for the rest. Return explicit error (empty + bool) on `wxCONV_FAILED`, not silent.

3. **Add BOM utilities in `TextEncoding`** — wrap `wxConvAuto::DetectBOM` / `wxConvAuto::GetBOMChars`:
   - `bomBytes(TextEncoding) -> std::span<const std::byte>` — calls `GetBOMChars` on mapped `wxBOM`
   - `bomLength(TextEncoding) -> size_t`
   Detection itself lives in `EncodingDetector` (TODO 5). No hand-rolled BOM tables.

### Foundation — `EncodingDetector` (dedicated class)

4. **Create `EncodingDetector` class** — new `src/lib/editor/EncodingDetector.hpp` + `.cpp`. Static entry points, zero state:
   - `struct DetectionResult { TextEncoding encoding; bool hadBom; };`
   - `static DetectionResult detect(const void* bytes, size_t len, TextEncoding fallback);` — full pipeline: `detectBom` → if none, `isLikelyUtf8` → else fallback
   - `static std::optional<TextEncoding> detectBom(const void* bytes, size_t len);` — wraps `wxConvAuto::DetectBOM`, maps `wxBOM → TextEncoding`
   - `static bool isValidUtf8(const void* bytes, size_t len);` — `wxConvUTF8.ToWChar(nullptr, 0, src, len) != wxCONV_FAILED`
   - `static bool isLikelyUtf8(const void* bytes, size_t len);` — valid + contains at least one non-ASCII byte
   - `static std::optional<EolMode> detectEol(const wxString& firstChunk);` — count `\r\n` / lone `\n` / lone `\r` in first 8 KB, majority wins. `nullopt` if empty / tie → caller falls back to configured default

5. **(reserved)** — EOL detection covered by TODO 4's `EncodingDetector::detectEol`. Keep slot for future detection hook (e.g. mixed-EOL warning) or drop if unused when implementing.

### Settings

6. **Add config defaults** — `editor.encoding = "UTF-8"`, `editor.eolMode = "CRLF"` in `resources/ide/config_win.ini` (and `config_lin.ini` if present — default to `LF` for Linux).

7. **Extend `GeneralPage`** — two `wxChoice` controls: Default Encoding, Default Line Endings. Populate from `TextEncoding::all()` / `EolMode::all()`. Read in ctor, write in `apply()`. Add language strings in `.fbl` files.

### Document state

8. **Extend `Document`** — add `m_encoding`, `m_eolMode`, `m_hadBom`. Getters + setters. Setter for encoding: just updates state (mark dirty). Setter for EOL: calls `editor->SetEOLMode()` + `editor->ConvertEOLs()` + marks dirty.

### Load / save

9. **Add `DocumentIO::load(path, defaultEncoding, defaultEol) -> LoadResult`** — new helper in `src/lib/editor/DocumentIO.{hpp,cpp}`. Raw bytes via `wxFile::ReadAll` into `wxMemoryBuffer`. Call `EncodingDetector::detect(bytes, len, defaultEncoding)` for encoding + BOM flag. Skip BOM bytes via `TextEncoding::bomLength`. Decode via `TextEncoding::decode` → wxString. `EncodingDetector::detectEol(text)` with fallback to `defaultEol`. Return `{text, encoding, hadBom, eolMode}`.

10. **Add `DocumentIO::save(path, text, encoding, eolMode, writeBom) -> bool`** — optional BOM via `bomBytes(encoding)`, convert EOLs in text before encode, encode via `TextEncoding::encode`, write bytes to `wxFile`. Atomic write (write to temp, rename) if the existing save path does it — otherwise match current behavior.

11. **Rewire `DocumentManager` load path** — replace `doc.getEditor()->LoadFile(path)` at `DocumentManager.cpp:93` with `DocumentIO::load` + `editor->SetText` + apply detected encoding/EOL to `Document` + `editor->SetSavePoint` + `editor->ConvertEOLs(detectedEol)`.

12. **Rewire `DocumentManager` save path** — replace `doc.getEditor()->SaveFile(path)` at `DocumentManager.cpp:113, 142` with `DocumentIO::save`, feeding document's encoding/EOL/BOM state.

13. **Rewire `CompilerManager` temp save** — `CompilerManager.cpp:84` currently uses `SaveFile` for a temp file. Decide: write as UTF-8 always (compiler-friendly) or preserve doc encoding. **Preserve doc encoding** — the FB compiler reads the user's original bytes. Route through `DocumentIO::save`.

14. **New-document defaults** — in `DocumentManager::newDocument` (or equivalent), seed `Document` with config defaults. `editor->SetEOLMode(toStcEolMode(doc.eolMode()))`.

### Editor wiring

15. **Remove hardcoded `SetEOLMode(wxSTC_EOL_LF)`** from `Editor.cpp:70`. EOL mode is set per-document from `Document` state after construction.

16. **Editor `setDocument` / init hook** — ensure `SetEOLMode` is called whenever a Document is assigned to an Editor.

### Status bar

17. **Expand status bar to 4 fields** — `UIManager.cpp:350`: `CreateStatusBar(4)`. Set widths: `{-1, 80, 80, 120}` (welcome stretch + fixed widths). Field 1 = line:col, field 2 = EOL, field 3 = encoding.

18. **Update `Editor::updateStatusBar`** — write EOL + encoding fields from active `Document` state alongside line:col. Also refresh on document focus change (already wired via `Editor::onFocus`).

19. **Create `EncodingMenu` helper class** — new `src/lib/ui/EncodingMenu.{hpp,cpp}` (adapted from Code::Blocks' `EncodingSelector` role):
    - `buildEolMenu(EolMode current) -> std::unique_ptr<wxMenu>` — LF / CRLF / CR radio entries, current checked. Uses `EolMode::all()` + display labels.
    - `buildEncodingMenu(TextEncoding current) -> std::unique_ptr<wxMenu>` — two sections:
      1. "Save with Encoding" — radio items, one per `TextEncoding::all()`, current checked. Convert-on-save semantics.
      2. Separator + "Reload with Encoding ..." submenu — same list, reload-from-disk semantics.
    - Emits command events with encoded payload (encoding value + mode = save/reload). UIManager handles the event.
    - Keeps menu strings localised via Lang.

20. **Status bar click handler** — in `UIManager`: bind `wxEVT_LEFT_DOWN` on the frame's status bar. Hit-test via `GetFieldRect`. Field 2 → `EncodingMenu::buildEolMenu(activeDoc.eolMode())` + `PopupMenu`. Field 3 → `EncodingMenu::buildEncodingMenu(activeDoc.encoding())` + `PopupMenu`. Dispatch result to active `Document` via `DocumentManager::getActiveDocument()`.

21. **Menu action handlers** — in `UIManager` (or delegated to `DocumentManager`):
    - EOL selection → `Document::setEolMode(newMode)` (triggers `ConvertEOLs` + `SetEOLMode` + dirty) → refresh status bar
    - Encoding save-with → `Document::setEncoding(newEncoding)` → mark dirty → refresh status bar
    - Encoding reload-with → if dirty, confirm discard via `wxMessageDialog` → `DocumentIO::load(path, newEncoding, currentEol)` → apply to Document/Editor → refresh status bar

### Tests

22. **Unit test `TextEncoding`** — decode/encode round-trip for all supported encodings with ASCII + representative non-ASCII samples. `bomBytes` / `bomLength` table. `fromString` / `toString` round-trip. `toFontEncoding` / `toStcEolMode` / `toWxBom` mappings.

23. **Unit test `EncodingDetector`** — BOM detection for all variants. UTF-8 validator positive/negative cases (invalid continuation bytes, overlong sequences, truncated, valid). `detectEol` each mode pure + mixed (majority wins) + empty + ties. `detect` full pipeline: BOM → UTF-8 → fallback.

24. **Unit test `DocumentIO::load`** — synthetic files in `tests/data/encoding/` covering BOM variants, non-BOM UTF-8, Windows-1252 sample, CP437 sample, mixed EOLs. Verify returned `LoadResult` state.

### Polish / optional

25. **Lossy-save warning** — when encoding a wxString to non-Unicode encoding drops characters, surface a dialog before overwriting. Use `wxCSConv` return values to detect loss.

26. **Remember per-file encoding/EOL in workspace state** — persist last-used encoding per file path across sessions. Stretch goal. Skip until asked.

27. **Language string coverage** — every new menu label + settings label added to `resources/ide/lang/*.fbl`. Verify no hardcoded English strings.

## Not doing (explicitly)

- Statistical encoding detection (`uchardet`, ICU). BOM + UTF-8 validate + config fallback is sufficient.
- Asian multi-byte encodings (Shift-JIS, GB2312, Big5, EUC-KR). `wxCSConv` supports them if a user requests later; not worth bundling now.
- UTF-32. No real-world FB sources use it.
- Per-file-type encoding defaults. Single global default covers the use case.
- Find/replace changes — wxSTC handles clipboard and search in its internal UTF-8; no encoding coupling.

## Suggested implementation order

TODOs 1–5 → 6–7 → 8 → 9–14 → 15–16 → 17–21 → 22–24 → 25 → 27.

Each block is independently testable. Ship in that order, review between blocks.
