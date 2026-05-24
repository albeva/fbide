#!/bin/bash
# Unix counterpart of build-wx.bat. Builds a static wxWidgets dist with
# the same module-trim list across Linux and macOS, so the cached dist
# can satisfy FBIde's link with no extra wx subsystems.
#
# Env vars (single source of truth in action.yml):
#   WX_SRC_DIR     — wxWidgets source checkout
#   WX_BUILD_DIR   — intermediate build tree (not cached)
#   WX_DIST_DIR    — install prefix (cached as the only artefact)
#   WX_BUILD_TYPE  — Debug / Release / RelWithDebInfo / MinSizeRel
#
# Linux: GTK3 toolkit; apt packages (GTK3 + headers wx links against)
# are installed by the action step before invoking this script —
# keeping the dependency list in YAML where Dependabot can see it.
#
# macOS: osx_cocoa toolkit; the dist is a universal binary covering
# both arm64 and x86_64. CMAKE_OSX_DEPLOYMENT_TARGET sets the runtime
# floor — must match LSMinimumSystemVersion in fbide's Info.plist
# (configured_files/Info.plist.in).
set -euo pipefail

# IPO is always off for the Linux/GCC wx build — known wx + GCC LTO
# failure (link-time codegen errors out of static wx libs) and the
# Windows/MSVC build is the one that benefits from it anyway. The
# clang/apple toolchain on macOS doesn't hit the same defect but
# leaving IPO off keeps both unix builds consistent and reproducible.

mkdir -p "${WX_BUILD_DIR}" "${WX_DIST_DIR}"

# Toolkit-specific args. The trim list below (wxUSE_* OFF) is shared so
# the resulting libs match FBIde's link footprint on both platforms.
case "$(uname -s)" in
    Darwin)
        TOOLKIT_ARGS=(
            -DwxBUILD_TOOLKIT=osx_cocoa
            -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
            -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
        )
        ;;
    *)
        TOOLKIT_ARGS=(
            -DwxBUILD_TOOLKIT=gtk3
        )
        ;;
esac

cmake -S "${WX_SRC_DIR}" -B "${WX_BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${WX_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${WX_DIST_DIR}" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    "${TOOLKIT_ARGS[@]}" \
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
