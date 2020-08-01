mkdir fbide_build

cmake -G "Ninja" \
      --parallel \
      -DCMAKE_BUILD_TYPE=Release \
      -DWXWIN= "%cd%\wx-dist" \
      -DWX_SOURCE_DIR="%cd%\wx-src" \
      -S . -B fbide_build

cd fbide_build
ninja
