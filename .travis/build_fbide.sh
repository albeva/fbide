mkdir fbide_build

cmake -G "NMake Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DWXWIN="$(pwd)/wx-dist" \
      -DWX_SOURCE_DIR="$(pwd)/wx-src" \
      -S . -B fbide_build

cd fbide_build
nmake
