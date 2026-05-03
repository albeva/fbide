# Integration plan: FBIde CI/CD

## Phase 0 — preflight decisions

- **VS 2026 availability.** `windows-latest` runner currently ships VS 2022. VS 2026 community released late 2025 — not yet on hosted runners as default toolchain. Options: (a) install at job start via `choco install visualstudio2026community --package-parameters "..."` (~5 min added per cold run, defeats some CI speed), (b) start with VS 2022, swap to 2026 once GitHub adds it. Recommend **(b)** — VS 2022 produces identical FBIde, no behaviour delta.
- **Pristine resources.** `resources/pristine/` mirrors `resources/ide/` minus dev state — neutral defaults for `config_*.ini`, empty `history.ini`, `changelog.md` (release notes), no CHM. Packaging copies straight from there, no sanitizer script needed.
- **Drift between `ide/` and `pristine/`.** Two parallel copies of `keywords.ini`, `layout.ini`, `shortcuts_*.ini`, locales, themes. User accepts manual sync at release time — no automated CI guard.
- **Drop `ide/changelog.txt`.** Old format, superseded by `pristine/changelog.md`. Remove from repo.
- **No CHM ship.** `FB-manual-1.10.1.chm` is FreeBASIC content, not editor. Pristine `helpFile=` empty. App falls back to wiki on missing CHM (already handled).
- **Bundle FreeBASIC compiler?** Initial release ships FBIde-only — simpler, no FBC version-pinning headache. FBC bundle (with CHM help file) deferred to later phase, see Phase 9.

## Phase 1 — branch + protection

- Create `develop` branch from current `main`.
- Push `main` protection rule: PRs only, requires `test.yml` green, no force-push, no direct commit.
- Default branch on GitHub repo: `develop`.
- Daily work targets `develop`. Releases are PR `develop → main` then tag.

## Phase 2 — wx prebuilt as cached composite action

Layout:

```
.github/
  actions/
    setup-wx/
      action.yml         (composite action)
      build-wx.bat       (vendored copy of build-msvc.bat, parametrised)
```

Source script: `C:\Users\Albert\Developer\wxwidgets\build-msvc.bat`. Vendor verbatim into the action so CI build matches local dev build exactly. Keep `.bat` (no PowerShell port) — script is short and already works under MSVC dev cmd.

Current flags (from `build-msvc.bat`):

```
-DwxBUILD_SHARED=OFF
-DwxBUILD_MONOLITHIC=OFF
-DwxBUILD_SAMPLES=OFF
-DwxBUILD_TESTS=OFF
-DwxUSE_GRID=OFF
-DwxUSE_WEBVIEW=OFF
-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

Possible further trimming (already disabled in old `build-githuhb.bat`): XRC, PROPGRID, RIBBON, MEDIACTRL, GLCANVAS, LIBWEBP, LIBTIFF, LIBJPEG, XML, REGEX, NANOSVG. Verify FBIde doesn't pull these before disabling. Defer trim until Phase 2 stable — current flags work.

`setup-wx` does:

1. Compute cache key = `wx-${{ runner.os }}-${{ inputs.wx-ref }}-${{ hashFiles('action.yml','build-wx.bat') }}`
2. `actions/cache/restore` keyed on it, target `${{ github.workspace }}/wxwidgets/dist`
3. On miss: `git clone --depth=1 --branch <wx-ref> https://github.com/wxWidgets/wxWidgets.git wxwidgets/src`, run `build-wx.bat` from `wxwidgets/`, `actions/cache/save`
4. Output dist path → set `wxWidgets_ROOT_DIR` env for downstream CMake calls (matches `CLAUDE.md` invocation)

Pin wx ref (e.g. `v3.3.0`) as input. Bumping wx = changing input = new cache key, no manual eviction.

Cache survives 7 days idle. If evicted, one cold rebuild (~5 min for trimmed wx config) — user already accepts this.

## Phase 3 — `test.yml` (PR + push gate)

Triggers: PR to `develop` or `main`, push to `develop`.

Path filter:
```yaml
paths:
  - 'src/**'
  - 'tests/**'
  - 'resources/**'
  - 'cmake/**'
  - 'CMakeLists.txt'
  - '.github/workflows/test.yml'
  - '.github/actions/setup-wx/**'
```

Steps:
1. `actions/checkout`
2. `setup-wx` composite (cached)
3. CMake configure Debug, `-DwxWidgets_ROOT_DIR=$WX_ROOT`
4. Ninja build
5. `ctest --output-on-failure`

Single job, ~3 min warm cache. Status check name `test` becomes required for PR merge.

## Phase 4 — `package.yml` (main → artifact)

Trigger: push to `main` (after merge).

Depends on `test.yml`: use `workflow_run: workflows: [test]; types: [completed]` and gate on `conclusion == success`. Or simpler: re-run tests inside `package.yml` first job. Recommend the latter — fewer moving parts.

Jobs:

1. **test** (same as `test.yml` body, Debug + ctest) — gates rest
2. **build-release** — needs test, uses `setup-wx`, builds Release
3. **package** — needs build-release:
   - Copy `bin/fbide.exe` + runtime DLLs (none currently — wx static, MSVC runtime: decide `/MT` static CRT or ship `vcredist`)
   - Copy `resources/pristine/*` → `package/ide/` (recursive)
   - Zip → `fbide-${{ version }}-win64.zip`
   - `actions/upload-artifact` with 30-day retention

CMake `install(DIRECTORY)` rule should use `resources/pristine/` as source (Phase 6). Lets `cmake --install` produce same layout locally for testing.

Artifact is sanity check / nightly download. Not a release.

## Phase 5 — `release.yml` (tag → installer)

Trigger: tag matching `v*` push.

Reuses test + build-release jobs. Adds:

- **installer** job after package:
  - Install Inno Setup: `choco install innosetup -y` (or pre-installed on `windows-latest` — verify)
  - Update `installer.iss` (see Phase 6)
  - `iscc installer.iss` produces `FBIde-${VERSION}-Setup.exe`
  - `softprops/action-gh-release` uploads zip + installer to GitHub Release page
  - Auto-extract release notes from `CHANGELOG.md` between tag headers (or manual via release page edit)

Tag = release. No tag = no installer published.

## Phase 6 — Inno Setup script refresh + CMake install target

### CMake install rules

Add to `CMakeLists.txt`:

```cmake
install(TARGETS fbide RUNTIME DESTINATION .)
install(DIRECTORY resources/pristine/ DESTINATION ide)
```

`cmake --install build/claude/release --prefix package` produces same layout as CI packaging step. Lets developers reproduce installer payload locally without CI.

### Inno Setup script

Source `installer.iss` from `C:\Users\Albert\public_html\installer.iss`. Move into repo at `installer/installer.iss`. Updates needed:

- `AppName=FBIde`, `AppVerName=FBIde {#FBIDE_VERSION}` — version injected via `iscc /DFBIDE_VERSION=0.5.0`
- `DefaultDirName={autopf}\FBIde` — modern Program Files default, not bare `\FreeBasic`
- Drop FreeBASIC component + tasks (per Phase 0 decision)
- `OutputBaseFilename=FBIde-{#FBIDE_VERSION}-Setup`
- `OutputDir=.` then upload from there
- File associations (.bas / .bi / .fbs) — keep, they were correct
- `LicenseFile=LICENSE` (top-level repo file, not buried in FreeBASIC subdir)
- Add `[Setup] WizardStyle=modern`, `PrivilegesRequired=lowest` (per-user install option)
- `Source: "package\*"` where `package\` is staged directory from Phase 4

Sign later — not blocking.

## Phase 7 — version source of truth

Pick one location. Candidates:

- `CMakeLists.txt` `project(fbide VERSION 0.5.0)` — natural for CMake
- Git tag — `git describe --tags --abbrev=0` injected at configure

Recommend **CMake project version** as canonical, tag must match. CI verifies on release: extract CMake version, compare to `${{ github.ref_name }}`, fail if mismatch. Prevents shipping `v0.6.0` tag with `0.5.0` binary.

## Phase 8 — sequencing

Implement in this order — each step shippable, reversible:

1. Branch model + protection (Phase 1) — no code, GitHub UI only
2. Drop `ide/changelog.txt` from repo (Phase 0). Optionally drop `ide/FB-manual-1.10.1.chm` from repo if too bulky for source tree (dev keeps local copy)
3. `setup-wx` composite action + `test.yml` (Phases 2 + 3) — biggest win, exercised on every PR
4. CMake install rules using `resources/pristine/` (Phase 6 first half) — verifiable locally with `cmake --install`
5. `package.yml` zipping (Phase 4) — verify packaging logic before installer
6. `installer.iss` refresh + manual local Inno Setup test (Phase 6 second half)
7. `release.yml` automating installer on tag (Phase 5)
8. CMake version + tag-matching guard (Phase 7)

Each phase reviewable as separate PR.

## Phase 9 — FBC bundle installer (later)

After initial FBIde-only releases ship cleanly, add second installer variant bundling FreeBASIC + CHM. Out of scope for v0.5; planning sketch only.

### Approach

- Keep FBIde-only installer as primary. Bundle is separate output.
- Pin FBC version (e.g. `1.10.1`) per FBIde release. Document mapping in `installer/README.md`.
- New workflow `release-bundle.yml` triggered by separate tag pattern (e.g. `v*-bundle`) or matrix job alongside main release.
- Steps:
  - Download FBC Windows zip from GitHub releases (`FreeBASIC-X.Y.Z-win64.zip`) via `gh release download` against `freebasic-dev/fbc` repo
  - Download FB-manual CHM from FBC release assets
  - Extract into staging dir alongside FBIde payload
  - Second Inno Setup script `installer-bundle.iss` (or shared script with `[Components]` enabled): components `fbide` + `fbc` + `chm`, all `Types: std`
  - On install, set `[fbc] path={app}\FreeBASIC\fbc.exe` and `helpFile=FB-manual-X.Y.Z.chm` in user config — Inno Setup `[INI]` section writes to `{userappdata}\fbide\config_win.ini` post-install
  - Output `FBIde-X.Y.Z-with-FreeBASIC-A.B.C-Setup.exe`

### Risks

- FBC release URL stability — `freebasic-dev/fbc` GitHub releases reliable, but pin asset name explicitly
- License bundling: FreeBASIC is GPL, FBIde is MIT. Bundle installer must include FBC license alongside FBIde license. Inno `[Components]`-conditional `LicenseFile` not supported — show combined license file
- Size: FBC + CHM ~70 MB extra. Bundle installer ~80–100 MB total
- FBC version drift: when FBC ships new release, CI matrix should rebuild bundle automatically (cron trigger or manual workflow_dispatch)

Defer until FBIde-only path is solid through 2–3 release cycles.

## Risks / open questions

- **wxWidgets static link size.** Current dev `dist/` is ~600 MB because it has Debug + Release + headers + samples. Trimmed Release-only with current flags should be ~80–120 MB cached. Verify on first cache run before declaring victory.
- **MSVC runtime distribution.** wx static doesn't bundle MSVC CRT. Either compile with `/MT` (static CRT, larger binary, no dep) or ship `vcredist_x64.exe` in installer `[Run]` section. Old installer didn't deal with this — VC6/older MSVC. Recommend `/MT` for simplicity.
- **MSVC version drift.** Hosted runner upgrades VS in place. Pin via `microsoft/setup-msbuild@v2` with explicit version when reproducibility matters.
- **Code signing.** Unsigned `.exe` triggers SmartScreen warning. Plan to acquire cert (Sectigo / DigiCert ~$200/yr) or use `signtool` with self-signed for now. Out of scope for v1 of pipeline.
- **macOS / Linux.** Not in this plan. Add as separate matrix entries to `test.yml` first (smoke), then packaging (`.AppImage` / `.dmg`) later.

## Cost

Public repo = unlimited free Actions minutes on GitHub-hosted runners. No infra to buy until you go private or self-host.
