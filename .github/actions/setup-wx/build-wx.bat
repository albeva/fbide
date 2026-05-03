@echo off
setlocal

REM Vendored copy of C:\Users\Albert\Developer\wxwidgets\build-githuhb.bat —
REM the trimmed-down configuration intended for CI artefacts. Disables every
REM wxWidgets module FBIde does not link against to keep the cached install
REM compact and shave cold-build time. When local trim flags change, update
REM this file in the same commit.
REM
REM Single-config build. Configured by env vars from action.yml — the build
REM type must match the downstream FBIde configure (Debug vs Release select
REM different static-lib variants and headers).
REM
REM Env vars (single source of truth in action.yml):
REM   WX_SRC_DIR     — wxWidgets source checkout
REM   WX_BUILD_DIR   — intermediate build tree (not cached)
REM   WX_DIST_DIR    — install prefix (cached as the only artefact)
REM   WX_BUILD_TYPE  — Debug / Release / RelWithDebInfo / MinSizeRel

REM IPO is incompatible with Debug codegen on MSVC — only enable for the
REM optimised configurations.
set WX_IPO=OFF
if /I "%WX_BUILD_TYPE%"=="Release" set WX_IPO=ON
if /I "%WX_BUILD_TYPE%"=="RelWithDebInfo" set WX_IPO=ON
if /I "%WX_BUILD_TYPE%"=="MinSizeRel" set WX_IPO=ON

if not exist "%WX_BUILD_DIR%" mkdir "%WX_BUILD_DIR%"
if not exist "%WX_DIST_DIR%" mkdir "%WX_DIST_DIR%"

cmake -S "%WX_SRC_DIR%" -B "%WX_BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=%WX_BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%WX_DIST_DIR%" ^
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=%WX_IPO% ^
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

cmake --build "%WX_BUILD_DIR%" --config %WX_BUILD_TYPE% --target install
if errorlevel 1 exit /b 1
