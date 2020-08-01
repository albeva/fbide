mkdir fbide_build

cmake -G "NMake Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DYAML_DIST_PATH="$(pwd)/yaml-dist/share/cmake"
      -DWXWIN="$(pwd)/wx-dist" \
      -S . -B fbide_build

cd fbide_build
nmake
