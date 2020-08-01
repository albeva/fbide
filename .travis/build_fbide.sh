mkdir fbide_build

cmake -G "NMake Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
      -DYAML_MSVC_SHARED_RT=OFF \
      -DWXWIN="$(pwd)/wx-dist" \
      -DWX_SOURCE_DIR="$(pwd)/wx-src" \
      -S . -B fbide_build

cd fbide_build
nmake
