@echo off

set VERSION=wxWidgets-3.1.5

set SRC=%cd%\%VERSION%
set BUILD=%cd%\%VERSION%-build
set INSTALL=%cd%\%VERSION%-dist

if not exist "%BUILD%" mkdir %BUILD%

cmake -G "Ninja" -S "%SRC%" -B "%BUILD%" ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DwxBUILD_SHARED=OFF
    
ninja -C "%BUILD%" install
