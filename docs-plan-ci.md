# Integration plan: FBIde CI/CD

This document tracks the state of FBIde's GitHub Actions pipeline and the
decisions behind it. Sections marked **DONE** are merged on `main`;
**PENDING** sections are the next deliveries.

## Phase 0 — preflight decisions (DONE)

- **Target platforms.** Windows 10 / 11 (64-bit, MSVC) and Linux. Earlier
  XP+ goal was dropped because MSVC-built binaries do not run on Win7 or
  earlier. No 32-bit Windows variant ships — keep code portable enough
  that 32-bit Linux still compiles, but no dedicated 32-bit artefact.
- **VS toolchain.** `windows-latest` runner provides VS 2022. VS 2026 not
  yet on hosted runners; not worth the install time. FBIde produces the
  same binary either way.
- **Pristine resources.** `resources/pristine/` mirrors `resources/ide/`
  minus dev state — neutral defaults for `config_*.ini`, empty
  `history.ini`, `changelog.md` (release notes), no CHM. Packaging copies
  straight from there, no sanitiser script needed.
- **Drift between `ide/` and `pristine/`.** Two parallel copies of
  `keywords.ini`, `layout.ini`, `shortcuts_*.ini`, locales, themes.
  Manual sync at release time; no automated CI guard.
- **Drop `ide/changelog.txt` and `ide/FB-manual-*.chm`** from the repo.
- **No CHM ship.** `FB-manual-1.10.1.chm` is FreeBASIC content, not
  editor. Pristine `helpFile=` empty. App falls back to wiki on missing
  CHM (already handled).
- **Bundle FreeBASIC compiler?** Initial release ships FBIde-only. FBC
  bundle deferred to Phase 9.
- **MSVC runtime.** `/MD` (dynamic CRT) for now — matches dev workstation
  build. `/MT` static CRT considered for installer simplicity but
  deferred; if installer ships, decide between `/MT` and bundling
  `vcredist`.

## Phase 1 — branch + protection (PARTIAL)

- `develop` branch created from `main`. **DONE**.
- Default branch on GitHub repo: `develop`. **DONE**.
- `main` protection rule (PRs only, requires `test` green, no force
  push): **deferred**. Direct pushes to `main` are currently used while
  iterating on the workflow itself; reintroduce protection once the
  pipeline is stable.

## Phase 2 — wx prebuilt as cached composite action (DONE)

Layout:

```
.github/
  actions/
    setup-wx/
      action.yml      composite action
      build-wx.bat    vendored copy of build-githuhb.bat
```

- **Action inputs:** `wx-ref` (default `v3.3.2`), `build-type` (default
  `Release`).
- **Outputs:** `wx-root` — absolute path to install prefix.
- **Cache:** key =
  `wx-${runner.os}-${wx-ref}-${build-type}-${hashFiles(action.yml,build-wx.bat)}`.
  Only the install prefix (`wxwidgets/dist`) is cached; sources and
  intermediate build trees are recreated on cold runs.
- **Build script.** Vendored from `build-githuhb.bat` — trim flags
  disable XRC, PROPGRID, RIBBON, MEDIACTRL, GLCANVAS, WEBP, TIFF, JPEG,
  XML, REGEX, NANOSVG. Keeps GRID, WEBVIEW off and HTML on. IPO toggled
  off for Debug (incompatible with Debug codegen on MSVC). Single-config
  build per cache entry — Debug and Release live in separate cache slots.
- **Line endings.** `.bat` / `.cmd` files are pinned to CRLF via
  `.gitattributes` so cmd.exe parses them correctly on Windows runners.

## Phase 3 — `test.yml` test gate (DONE)

- **Triggers:** `pull_request` to `main`, `push` to `main`, and
  `workflow_dispatch` (manual; honours a `build_type` input — default
  `Release`, choices `Release` / `Debug` / `RelWithDebInfo`).
- **Path filter:** `src/**`, `tests/**`, `resources/**`, `cmake/**`,
  `CMakeLists.txt`, `configured_files/**`, the workflow file, the
  `setup-wx` action.
- **Concurrency:** older runs on the same ref are cancelled when a new
  one starts.
- **Node runtime:** `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24=true` at workflow
  scope keeps any straggler actions on Node 24 ahead of GitHub's June
  2026 default switch.
- **Action versions:**
  - `actions/checkout@v5` (Node 24)
  - `actions/cache/{restore,save}@v5` (Node 24)
  - `lukka/get-cmake@latest` (Node 24) — installs CMake AND Ninja, so
    `seanmiddleditch/gha-setup-ninja` is no longer needed (archived May
    2025 anyway).
  - `ilammy/msvc-dev-cmd@v1.13.0` — still Node 20, only one left riding
    the env-var override. Replace with inline `vcvars64.bat` invocation
    if upstream stays silent through 2026.
- **Single job** `build-and-test`:
  1. Checkout
  2. MSVC dev environment (`x64`)
  3. CMake + Ninja via `lukka/get-cmake@latest`, `cmakeVersion: latest`
     (hosted runner ships 3.31; FBIde requires 4.0+).
  4. `setup-wx` composite (cached).
  5. Configure: `cmake -G Ninja -B build/ci -DCMAKE_BUILD_TYPE=$BUILD_TYPE
     -DWXWIN=$WX_ROOT`. `WXWIN` switches `cmake/wxwidgets.cmake` to
     CONFIG mode (the `cmake --install` layout we ship); the alternate
     `wxWidgets_ROOT_DIR` legacy path expects the prebuilt-binaries
     layout we don't ship.
  6. Build the default target (fbide.exe + tests). `fbide_lib` has IPO
     enabled for Release, so the link goes through LTCG once — paying
     that cost lets the upcoming packaging step reuse `fbide.exe` from
     the same build instead of configuring + linking the project a
     second time on tag push.
  7. `ctest --output-on-failure`. `enable_testing()` at top-level
     `CMakeLists.txt` is what makes `gtest_discover_tests` registrations
     visible to ctest — without it, ctest reports "No tests were found".

## Phase 4 — packaging on tag push (PENDING, integrated into `test.yml`)

Same workflow run that ran the test gate also performs the packaging
when the trigger is a tag push matching `v*`. No artifact passing
between jobs; reuses the build from step 6 above.

Steps added at the bottom of the job, gated by
`if: startsWith(github.ref, 'refs/tags/v')`:

1. Verify the tag matches the configured version. The build emits
   `<build>/version.txt` containing `${FBIDE_FULL_VERSION}` (see
   Phase 7); CI compares `v$(cat version.txt)` to `${{ github.ref_name }}`
   and fails on mismatch.
2. `cmake --install build/ci --prefix package`.
3. Zip the staged `package/` tree to
   `fbide-${VERSION}-win64.zip` (uses the full version including any
   `.alpha-1` suffix).
4. `softprops/action-gh-release@v2` publishes the GitHub Release. The
   `prerelease` flag is auto-set when the tag contains a `-` (i.e. has
   any `.alpha-N`/`.beta-N`/`.rc-N` suffix).

CMake install rules (added to top-level `CMakeLists.txt`):

```cmake
install(TARGETS fbide RUNTIME DESTINATION .)
install(DIRECTORY resources/pristine/ DESTINATION ide)
```

Same command (`cmake --install build/<dir> --prefix <somewhere>`)
reproduces the installer payload locally for sanity checking.

There is no separate `package.yml` artifact-on-push workflow — added
later if useful.

## Phase 5 — installer (.exe via Inno Setup) (DEFERRED)

Out of scope for the first release pipeline. Plain zip + GitHub
Release is the v1 deliverable. When the installer phase lands, it
reuses the staged `package/` tree from Phase 4 as input to
`installer.iss`:

- `AppName=FBIde`, version injected via `iscc /DFBIDE_VERSION=...`.
- `DefaultDirName={autopf}\FBIde`, drop FreeBASIC component.
- `LicenseFile=LICENSE`, `WizardStyle=modern`,
  `PrivilegesRequired=lowest`.
- File associations `.bas` / `.bi` / `.fbs` retained from old script.
- Sign later — not blocking.

## Phase 6 — CMake install rules (PENDING — small)

See Phase 4. Two `install(...)` calls in `CMakeLists.txt`. No further
infrastructure. `cmake --install` handles everything.

## Phase 7 — versioning (PENDING — implementing)

### Source of truth

`CMakeLists.txt` `project(fbide VERSION 0.5.0 ...)` is canonical for the
numeric triple. CMake only accepts numeric `MAJOR.MINOR.PATCH[.TWEAK]`
in `project(VERSION ...)`, so the pre-release marker is held in two
extra cache vars:

```cmake
set(FBIDE_VERSION_TAG "" CACHE STRING "Pre-release tag (alpha|beta|rc)")
set(FBIDE_VERSION_TWEAK 1 CACHE STRING "Pre-release iteration number")
```

`FBIDE_FULL_VERSION` is composed from these:

- empty tag → `${PROJECT_VERSION}` (e.g. `0.5.0`)
- non-empty → `${PROJECT_VERSION}.${TAG}-${TWEAK}` (e.g. `0.5.0.alpha-1`)

A small CMake mapping converts the lowercase tag string to its enum
case name; unknown tag values are passed through verbatim so a typo
(e.g. `foo`) becomes `Version::Tag::foo` and fails at compile time —
no allowlist to maintain.

### `Version` class

`enum class Tag : uint8_t { None, Alpha, Beta, ReleaseCandidate }` lives
nested inside `Version` (`Version::Tag`). The class:

- Constructor takes `int major, int minor, int patch, Tag tag = None,
  int tweak = 0`.
- `asString()` formats `MAJOR.MINOR.PATCH[.TAG-TWEAK]`.
- String parser accepts the same shape (`0.5.0`, `0.5.0.alpha`,
  `0.5.0.alpha-1`, `0.5.0.rc-2`). Unknown tags silently fall back to
  `Tag::None` — only strings the project itself generates are parsed,
  so the existing tolerant-parser style is preserved.
- `operator<=>`: compare numeric triple first, then tag rank with
  `None` highest (final > rc > beta > alpha), then tweak.

`Version.hpp` does not include `config.hpp`. The `static fbide()`
factory body moves to `Version.cpp` (loses `constexpr`). All current
call sites are runtime — checked: `BuildTask.cpp`, `App.cpp`,
`AboutDialog.cpp`. No dependents demand `constexpr`.

### Configured files

`config.hpp.in`:

```cpp
#include "config/Version.hpp"

namespace fbide::cmake {
    struct Project final {
        const char* name;
        const char* description;
        const char* version;
        int major, minor, patch;
        Version::Tag tag;
        int tweak;
    };
    static constexpr Project project {
        .name        = "@PROJECT_NAME@",
        .description = "@PROJECT_DESCRIPTION@",
        .version     = "@PROJECT_VERSION@",
        .major       = @PROJECT_VERSION_MAJOR@,
        .minor       = @PROJECT_VERSION_MINOR@,
        .patch       = @PROJECT_VERSION_PATCH@,
        .tag         = Version::Tag::@FBIDE_VERSION_TAG_ENUM@,
        .tweak       = @FBIDE_VERSION_TWEAK_OR_ZERO@,
    };
}
```

`version.rc.in` gains a full-version literal for the Windows resource
table.

`configured_files/CMakeLists.txt` also writes `${CMAKE_BINARY_DIR}/version.txt`
containing `${FBIDE_FULL_VERSION}` — read by CI for tag-matching.

### Consumers

- `App::showVersion()` — emits the full version (with tag/tweak when
  set).
- `AboutDialog` — same.
- `BuildTask` compiler-log preamble — same.

### Tag verification

CI reads `<build>/version.txt`, prefixes with `v`, compares to
`github.ref_name`. Mismatch fails the release. Albert bumps
`PROJECT_VERSION` / `FBIDE_VERSION_TAG` / `FBIDE_VERSION_TWEAK` in
`CMakeLists.txt` per release, commits, then tags `v<full-version>`.

## Phase 8 — sequencing

Revised from the original plan; reflects what has shipped and what is
next.

1. Branch model + protection (Phase 1) — partial; protection deferred.
2. Drop `ide/changelog.txt` + `ide/FB-manual-*.chm` — DONE.
3. `setup-wx` composite + `test.yml` — DONE.
4. Versioning (Phase 7) — IN PROGRESS, current PR.
5. CMake install rules + tag-trigger packaging block in `test.yml`
   (Phases 4 + 6) — same PR or follow-up.
6. Re-add `main` protection once the pipeline is stable.
7. Installer (`installer.iss`) — Phase 5, future PR.
8. FBC bundle — Phase 9, future PR after 2–3 release cycles.

## Phase 9 — FBC bundle installer (later)

Unchanged from original plan. Bundle FreeBASIC + CHM with FBIde in a
second installer variant (separate `release-bundle.yml`, separate tag
pattern e.g. `v*-bundle`). Out of scope for v1 of the pipeline.

## Risks / open questions

- **wxWidgets static link size.** Cache currently around 100 MB with
  trimmed config (Release-only). Verified on first cache run.
- **MSVC runtime distribution.** `/MD` for now. `/MT` switch deferred to
  installer phase.
- **MSVC version drift.** Hosted runner upgrades VS in place.
  `ilammy/msvc-dev-cmd@v1.13.0` pins the action; the underlying VS
  install is whatever GitHub ships.
- **Code signing.** Unsigned `.exe` triggers SmartScreen warning.
  Acquire cert (~$200/yr) or self-signed when installer phase lands.
  Out of scope for the zip pipeline.
- **macOS / Linux.** Not in this plan. Add as separate matrix entries
  to `test.yml` first (smoke), then packaging later.

## Cost

Public repo = unlimited free Actions minutes on GitHub-hosted runners.
No infra to buy until private repo or self-hosted runners.
