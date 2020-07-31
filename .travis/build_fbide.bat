call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86

mkdir fbide_build

cmake -G "Ninja" ^
      --parallel ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DWXWIN= "%cd%\wx-dist" ^
      -DWX_SOURCE_DIR="%cd%\wx-src" ^
      -S fbide -B fbide_build

cd fbide_build
ninja
