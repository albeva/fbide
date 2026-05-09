#!/bin/bash
# Linux counterpart of build-wx.bat. Builds a static wxWidgets dist
# against GTK3 with the same module-trim list, so the cached dist can
# satisfy FBIde's link with no extra wx subsystems.
#
# Env vars (single source of truth in action.yml):
#   WX_SRC_DIR     — wxWidgets source checkout
#   WX_BUILD_DIR   — intermediate build tree (not cached)
#   WX_DIST_DIR    — install prefix (cached as the only artefact)
#   WX_BUILD_TYPE  — Debug / Release / RelWithDebInfo / MinSizeRel
#
# Apt packages (GTK3 + headers wx links against) are installed by the
# action step before invoking this script — keeping the dependency list
# in YAML where Dependabot can see it.
set -euo pipefail

# IPO is always off for the Linux/GCC wx build — known wx + GCC LTO
# failure (link-time codegen errors out of static wx libs) and the
# Windows/MSVC build is the one that benefits from it anyway.

mkdir -p "${WX_BUILD_DIR}" "${WX_DIST_DIR}"

cmake -S "${WX_SRC_DIR}" -B "${WX_BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${WX_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${WX_DIST_DIR}" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DwxBUILD_TOOLKIT=gtk3 \
    -DwxBUILD_SHARED=OFF \
    -DwxBUILD_MONOLITHIC=OFF \
    -DwxBUILD_SAMPLES=OFF \
    -DwxBUILD_TESTS=OFF \
    -DwxUSE_GRID=OFF \
    -DwxUSE_WEBVIEW=OFF \
    -DwxUSE_HTML=ON \
    -DwxUSE_XRC=OFF \
    -DwxUSE_PROPGRID=OFF \
    -DwxUSE_RIBBON=OFF \
    -DwxUSE_MEDIACTRL=OFF \
    -DwxUSE_GLCANVAS=OFF \
    -DwxUSE_LIBWEBP=OFF \
    -DwxUSE_LIBTIFF=OFF \
    -DwxUSE_LIBJPEG=OFF \
    -DwxUSE_XML=OFF \
    -DwxUSE_REGEX=OFF \
    -DwxUSE_NANOSVG=OFF

cmake --build "${WX_BUILD_DIR}" --target install
