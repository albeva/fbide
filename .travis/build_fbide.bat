call dev_cmd.bat

mkdir fbide_build

cmake -G "Ninja" ^
      --parallel ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DWXWIN= "%cd%\libs\wxWidgets\dist" ^
      -DWX_SOURCE_DIR="%cd%\libs\wxWidgets\wxWidgets-3.1.4" ^
      -S fbide -B fbide_build

cd fbide_build
ninja
