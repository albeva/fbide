"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86
VsDevCmd.bat -test
cmake -G "NMake Makefiles" -DwxBUILD_USE_STATIC_RUNTIME=ON -DwxBUILD_SHARED=OFF -DwxBUILD_SAMPLES=OFF -DwxBUILD_DEMOS=OFF -DwxBUILD_CXX_STANDARD=17 -DwxBUILD_COMPATIBILITY=3.1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=wx-dist -S wx-src -B wx-build
cd wx-build
nmake install
