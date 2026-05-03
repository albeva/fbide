@echo off
setlocal

REM Vendored copy of C:\Users\Albert\Developer\wxwidgets\build-githuhb.bat —
REM the trimmed-down configuration intended for CI artefacts. Disables every
REM wxWidgets module FBIde does not link against to keep the cached install
REM compact and shave cold-build time. When local trim flags change, update
REM this file in the same commit.
REM
REM Builds BOTH Debug and Release configs into the same dist prefix so that
REM downstream FBIde jobs can pick either build type — Debug FBIde requires
REM the wxmsw33ud_*.lib variant + mswud/wx/setup.h header, Release uses
REM mswu/. Static libs for both configs coexist side-by-side in lib/.
REM
REM Paths are passed in by action.yml via env vars (single source of truth):
REM   WX_SRC_DIR    — wxWidgets source checkout
REM   WX_BUILD_DIR  — intermediate build tree (not cached)
REM   WX_DIST_DIR   — install prefix (cached as the only artefact)

if not exist "%WX_BUILD_DIR%" mkdir "%WX_BUILD_DIR%"
if not exist "%WX_DIST_DIR%" mkdir "%WX_DIST_DIR%"

REM Configure once (multi-config Visual Studio generator), then build+install
REM each config in turn into the shared prefix.
cmake -S "%WX_SRC_DIR%" -B "%WX_BUILD_DIR%" ^
    -DCMAKE_INSTALL_PREFIX="%WX_DIST_DIR%" ^
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ^
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

cmake --build "%WX_BUILD_DIR%" --config Release --target install
if errorlevel 1 exit /b 1

cmake --build "%WX_BUILD_DIR%" --config Debug --target install
if errorlevel 1 exit /b 1
