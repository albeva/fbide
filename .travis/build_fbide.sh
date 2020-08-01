mkdir fbide_build

cmake -G "NMake Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DFBIDE_USE_SHARED_LIBS=OFF \
      -DWXWIN="$(pwd)/wx-dist" \
      -S . -B fbide_build

cd fbide_build
nmake
