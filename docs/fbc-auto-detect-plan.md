# Implementation plan — Auto-detect fbc

Windows-only (`wxMSW`) feature that auto-detects an installed FreeBASIC
compiler and generates OS/variant-appropriate compiler configurations,
applied to the live config so the Compiler settings page just reloads.

## Goals

- One-click detection of `fbc` from the system `PATH` or a chosen folder.
- Generate a complete, valid `[compiler]` config tree (canonical Default +
  per-variant configs) and install it wholesale.
- Keep the Compiler settings page changes minimal: it only triggers
  detection and reloads from the produced `Value`.

## Component split

| Component | Responsibility |
|-----------|----------------|
| **`FbcAutoDetect`** (new, `src/lib/compiler/FbcAutoDetect.{hpp,cpp}`, `#ifdef __WXMSW__`) | Owns detection + the whole interactive dialog flow. Returns a ready-to-install `[compiler]` `Value` subtree, or `nullopt`. Pure cores split out for unit tests. |
| **`SettingsDialog`** (`src/lib/settings/SettingsDialog.cpp`) | Adds the **Auto detect** button to the OK/Cancel row, left-aligned; shows it only on the Compiler tab; routes its click to the Compiler page. |
| **`CompilerPage`** (`src/lib/settings/panels/CompilerPage.{hpp,cpp}`) | One new public method `autoDetect()`: install returned `Value`, reload catalog, refresh list + combobox. Reuses existing `refreshList` / `selectSlug` / `loadSelectedConfig`. |
| **`ConfigManager`** (`src/lib/config/ConfigManager.{hpp,cpp}`) | Small read-only getter exposing the pristine `baseline` for a category (needed by the overwrite predicate). |

**Why install a whole `Value`, not CRUD calls:** `CompilerPage::create()`
snapshots `[compiler]` and `cancel()` restores it. Replacing
`config()["compiler"]` wholesale therefore rides the existing snapshot
model for free — OK persists the detected config, Cancel undoes it.

## `FbcAutoDetect` API

```cpp
enum class FbcArch : std::uint8_t { Win32, Win64 };
struct FbcVariant { std::filesystem::path exe; FbcArch arch; };

class FbcAutoDetect {
public:
    explicit FbcAutoDetect(Context& ctx);

    /// Full interactive flow: overwrite-confirm -> PATH/Browse -> detect.
    /// Returns the new [compiler] subtree to install, or nullopt when the
    /// user cancels or nothing valid is found (error already shown).
    auto run(wxWindow* parent) -> std::optional<Value>;

    // --- pure, unit-tested ---
    static auto parseArch(const wxString& versionLine) -> std::optional<FbcArch>;
    static auto buildCompilerValue(std::span<const FbcVariant>, bool osIs64) -> Value;

    /// Probe is injectable so tests stub `--version` without real binaries.
    using Probe = std::function<wxString(const std::filesystem::path&)>;
    static auto detectVariants(const std::filesystem::path& folder, const Probe&)
        -> std::vector<FbcVariant>;
};
```

## Detection algorithm

1. **PATH search** — `wxPathList::AddEnvList("PATH")` +
   `FindAbsoluteValidPath` for `fbc.exe`, `fbc32.exe`, `fbc64.exe`; first
   hit yields its containing folder.
2. **Variant detect in folder** — candidate priority order
   `fbc64.exe`(→Win64), `fbc32.exe`(→Win32), `fbc.exe`(arch from
   `--version`). Run `--version` on each to confirm it is runnable; named
   variants take arch from filename but must still run. Dedup by arch
   (named beats plain `fbc.exe`). Empty result → error
   *"Unable to detect fbc compilers in given folder"*, abort.
3. **OS arch** — `wxIsPlatform64Bit()`.
4. **Version probe** — reuse the existing pattern from
   `CompilerManager::probeCompilerVersion`:
   `wxExecute("\"" + exe + "\" --version", output)`. `parseArch` scans the
   first line for `win64`/`64bit` vs `win32`/`32bit`.

## Generated `Value` schema

Example: 64-bit OS with both `fbc32.exe` + `fbc64.exe` present.

```ini
[compiler]
path=<folder>\fbc64.exe          ; OS-arch binary: 64 -> fbc64, else fbc32
compileCommand="<$fbc>" "<$file>"
runCommand="<$file>" <$param>
terminal=cmd /C
showInMenu=0                     ; Default hidden from the menu
active=cfg-4                     ; console config matching OS arch
nextSlugIndex=5

[compiler/cfg-1]
name=Win32 GUI
path=<folder>\fbc32.exe
compileCommand="<$fbc>" -target win32 -s gui "<$file>"
showInMenu=1
order=1

[compiler/cfg-2]
name=Win32 Console
path=<folder>\fbc32.exe
compileCommand="<$fbc>" -target win32 -s console "<$file>"
showInMenu=1
order=2

[compiler/cfg-3]
name=Win64 GUI
path=<folder>\fbc64.exe
compileCommand="<$fbc>" -target win64 -s gui "<$file>"
showInMenu=1
order=3

[compiler/cfg-4]
name=Win64 Console
path=<folder>\fbc64.exe
compileCommand="<$fbc>" -target win64 -s console "<$file>"
showInMenu=1
order=4
```

Rules:

- Per-arch GUI + Console pairs, **only for installed/valid arches**. A lone
  win64 `fbc.exe` yields just Win64 GUI/Console.
- Variant order is fixed: Win32 GUI, Win32 Console, Win64 GUI, Win64 Console.
- Canonical Default: `path` = OS-arch binary (64→fbc64 else fbc32; on 32-bit
  OS use fbc32, else fall back to whatever exists); `showInMenu=0`;
  command/run/terminal as shown.
- cfg sections set `name` / `path` / `compileCommand` / `showInMenu` /
  `order`; they **omit** `runCommand` and `terminal` so those inherit the
  Default.
- `active` = the console config of the canonical arch (falls back to the
  available arch's console when the exact OS-arch binary is absent).
- Config names are **hardcoded English** ("Win32 GUI", …) — not localized.
- Commands stored raw in memory (`"<$fbc>" …`); the INI writer handles
  quote-escaping on save. Bools as `0`/`1` (matches existing
  `setShowInMenu`). `nextSlugIndex` set past the last cfg so a later Add
  won't collide.

## Dialog flow (inside `FbcAutoDetect::run`)

1. **Overwrite-confirm** — only when existing settings present (predicate
   below). `wxYES_NO`; abort on No.
2. **Found in PATH** — `wxRichMessageDialog` with `wxYES_NO|wxCANCEL`,
   `SetYesNoCancelLabels(Yes, Browse, Cancel)`: **Yes** = use the PATH
   folder, **Browse** (No-slot) = folder picker, **Cancel** = abort, no
   changes. (Resolves the spec's "Yes/Browse/Cancel" button list vs its
   "No do nothing" prose — Cancel is the do-nothing path.)
3. **Not found / Browse** — `wxDirDialog` titled "Locate folder…";
   re-prompt when the chosen folder has no `fbc*.exe`; abort on cancel.
4. `detectVariants` → `buildCompilerValue` → return the subtree.

## Overwrite-warning predicate

"Existing settings present" = any user config exists, **or** the Default's
`path` / `compileCommand` / `runCommand` / `terminal` differs from the
shipped pristine value. Compared against `ConfigManager` **baseline** (the
parsed pristine `config_win.ini`), NOT the `kDefault*` code constants —
those constants differ from the shipped values (`kDefaultCompileTemplate`
lacks `-s console`; `kDefaultRunTemplate` carries a `<$terminal>` prefix
pristine-win does not), so comparing to them would mis-flag a pristine
install as modified.

```cpp
const auto& cur  = config().at("compiler");          // root (baseline + overlay)
const auto& base = baseline(Config).at("compiler");   // pristine config_win.ini
bool warn = catalog().all().size() > 1                // any user cfg-N
    || cur.get_or("path","")           != base.get_or("path","")
    || cur.get_or("compileCommand","") != base.get_or("compileCommand","")
    || cur.get_or("runCommand","")     != base.get_or("runCommand","")
    || cur.get_or("terminal","")       != base.get_or("terminal","");
```

`path` needs no special case: its baseline is empty, so `path != baseline`
≡ `path non-empty`. Untouched pristine (empty path, shipped commands +
terminal, no user cfgs) → **no warning**.

## SettingsDialog changes (`#ifdef __WXMSW__`)

- Replace `mainSizer->Add(btnSizer, …)` with a horizontal row sizer:
  `[Auto detect] —stretch— [Std OK/Cancel]`.
- Button uses a custom ID (not a `wxID_*`) so it never triggers `EndModal`;
  bind its click → `m_compilerPage->autoDetect()`.
- Bind `wxEVT_NOTEBOOK_PAGE_CHANGED` → `Show(sel == Page::Compiler)` +
  `Layout()`. Set initial visibility from `m_notebook->GetSelection()`
  (a deep-link can open the Compiler tab directly).

## CompilerPage changes

New public method (body `#ifdef __WXMSW__`):

```cpp
void CompilerPage::autoDetect() {
    FbcAutoDetect detector(getContext());
    auto generated = detector.run(this);
    if (!generated.has_value()) return;          // cancelled / nothing found
    auto& compiler = getContext().getConfigManager().config()["compiler"];
    compiler = std::move(*generated);
    catalog().reload();
    refreshList();
    m_selectedSlug = catalog().activeSlug();
    selectSlug(m_selectedSlug);
    loadSelectedConfig();
    getContext().getCompilerManager().refreshConfigurationCombo();
}
```

Note: `apply()` calls `commitFieldOverrides()` first, but after detection
the widgets already mirror the active config, so the re-commit round-trips
cleanly — no corruption.

## ConfigManager change

Expose the pristine baseline (currently in the private `Entry` struct, no
public accessor):

```cpp
[[nodiscard]] auto baseline(Category category) const -> const Value&;
```

## Locale keys

Add under `[dialogs/settings/compiler]` in `resources/pristine/locales/en.ini`
**and** the test fixture `tests/data/resources/ide/locales/en.ini`:
`autoDetect` (button), `autoOverwriteConfirm`, `autoFoundInPath`,
`autoBrowse`, `autoSelectFolder`, `autoNoFbc` (+ titles as needed). Plain
Yes/No use native wx labels (no keys). No variant-name keys (names
hardcoded).

## Tests

`tests/unit/FbcAutoDetectTests.cpp` (gtest), following the `TempDir` +
`makeConfig` pattern from `CompilerConfigCatalogTests`:

- `parseArch` — win32 / win64 / 64bit / garbage lines.
- `buildCompilerValue` — assert keys, `active`, `order`, `showInMenu`,
  inheritance (no `runCommand`/`terminal` on cfgs) for each arch combo and
  for 32-/64-bit OS.
- `detectVariants` — `TempDir` writes fake `fbc*.exe`; stub `Probe` returns
  canned version strings; assert dedup, priority, and empty-folder result.

`run()` (dialog orchestration) is not unit-tested.

## Platform guarding

`FbcAutoDetect.cpp` body, the SettingsDialog button block, and
`CompilerPage::autoDetect()` body all under `#ifdef __WXMSW__`. Add the new
source + test files to CMake.

## Build order (TODO sequence)

1. `ConfigManager::baseline()` getter.
2. `FbcAutoDetect` types + pure cores (`parseArch`, `buildCompilerValue`,
   `detectVariants`) + CMake.
3. `FbcAutoDetect::run()` — dialogs + overwrite predicate (uses baseline).
4. `CompilerPage::autoDetect()` + wiring.
5. SettingsDialog — button row, tab visibility, click routing.
6. Locale keys — en.ini + test fixture.
7. `FbcAutoDetectTests` + CMake.
8. Build (`build/claude/{debug,release}`) + manual verify on Windows.

## Decisions log

- **Overwrite trigger**: any user config OR Default's
  path/compileCommand/runCommand/terminal differs from pristine baseline.
- **Expected defaults source**: `ConfigManager` baseline (= pristine
  `config_win.ini`), not the `kDefault*` constants.
- **Variant config names**: hardcoded English, not localized.
- **PATH dialog buttons**: Yes (use) / Browse / Cancel (abort).
- **FbcAutoDetect owns the dialogs** (keeps CompilerPage minimal); pure
  cores extracted for tests.
