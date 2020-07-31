mkdir wx-build
mkdir wx-dist

cmake -G "NMake Makefiles" -DwxBUILD_USE_STATIC_RUNTIME=ON \
                           -DwxBUILD_SHARED=OFF \
                           -DwxBUILD_SAMPLES=OFF \
                           -DwxBUILD_DEMOS=OFF \
                           -DwxBUILD_CXX_STANDARD=17 \
                           -DwxBUILD_COMPATIBILITY=3.1 \
                           -DCMAKE_BUILD_TYPE=Release \
                           -DCMAKE_INSTALL_PREFIX=wx-dist \
                           -S wx-src -B wx-build

cd wx-build
nmake install
