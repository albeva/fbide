@echo off
setlocal

REM Vendored copy of C:\Users\Albert\Developer\wxwidgets\build-githuhb.bat —
REM the trimmed-down configuration intended for CI artefacts. Disables every
REM wxWidgets module FBIde does not link against to keep the cached install
REM compact and shave cold-build time. When local trim flags change, update
REM this file in the same commit.
REM
REM wxWidgets' build is per-config, so we configure + build TWICE:
REM   - Release into <build-dir>/Release with IPO on
REM   - Debug   into <build-dir>/Debug   with IPO off
REM Both passes install to the same DIST prefix; static libs land side-by-side
REM in lib/ (e.g. wxmsw33u_core.lib + wxmsw33ud_core.lib) and per-config
REM headers under mswu/wx/setup.h and mswud/wx/setup.h.
REM
REM Paths are passed in by action.yml via env vars (single source of truth):
REM   WX_SRC_DIR    — wxWidgets source checkout (shared)
REM   WX_BUILD_DIR  — intermediate build root (per-config subdir, not cached)
REM   WX_DIST_DIR   — install prefix (cached as the only artefact)

if not exist "%WX_BUILD_DIR%" mkdir "%WX_BUILD_DIR%"
if not exist "%WX_DIST_DIR%" mkdir "%WX_DIST_DIR%"

call :build Release ON || exit /b 1
call :build Debug OFF || exit /b 1
exit /b 0


:build
setlocal
set BUILD_TYPE=%~1
set IPO=%~2
set CFG_BUILD_DIR=%WX_BUILD_DIR%\%BUILD_TYPE%

if not exist "%CFG_BUILD_DIR%" mkdir "%CFG_BUILD_DIR%"

cmake -S "%WX_SRC_DIR%" -B "%CFG_BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%WX_DIST_DIR%" ^
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=%IPO% ^
    -DwxBUILD_SHARED=OFF ^
    -DwxBUILD_MONOLITHIC=OFF ^
    -DwxBUILD_SAMPLES=OFF ^
    -DwxBUILD_TESTS=OFF ^
    -DwxUSE_GRID=OFF ^
    -DwxUSE_WEBVIEW=OFF ^
    -DwxUSE_HTML=ON ^
    -DwxUSE_XRC=OFF ^
    -DwxUSE_PROPGRID=OFF ^
    -DwxUSE_RIBBON=OFF ^
    -DwxUSE_MEDIACTRL=OFF ^
    -DwxUSE_GLCANVAS=OFF ^
    -DwxUSE_LIBWEBP=OFF ^
    -DwxUSE_LIBTIFF=OFF ^
    -DwxUSE_LIBJPEG=OFF ^
    -DwxUSE_XML=OFF ^
    -DwxUSE_REGEX=OFF ^
    -DwxUSE_NANOSVG=OFF
if errorlevel 1 exit /b 1

cmake --build "%CFG_BUILD_DIR%" --config %BUILD_TYPE% --target install
if errorlevel 1 exit /b 1

endlocal & exit /b 0
