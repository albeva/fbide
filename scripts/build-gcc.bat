@echo off
setlocal

set SRC_DIR=src
set BUILD_TYPE=Debug
set BUILD_DIR="%CD%\build\gcc\%BUILD_TYPE%"
set DIST_DIR="%CD%\dist"

REM Create folders
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %DIST_DIR% mkdir %DIST_DIR%

REM Configure
cmake -S %SRC_DIR% -B %BUILD_DIR% ^
    -DCMAKE_C_COMPILER=gcc ^
    -DCMAKE_CXX_COMPILER=g++ ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=%DIST_DIR% ^
    -DwxBUILD_SHARED=OFF ^
    -DwxBUILD_MONOLITHIC=OFF ^
    -DwxBUILD_SAMPLES=OFF ^
    -DwxBUILD_TESTS=OFF ^
    -DwxUSE_WEBVIEW=OFF

if errorlevel 1 exit /b 1

REM Build + Install
cmake --build %BUILD_DIR% --config %BUILD_TYPE% --target install
