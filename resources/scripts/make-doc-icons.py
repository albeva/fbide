#!/usr/bin/env python3
"""Generate FBIde application, document, and installer icons from the Inkscape
SVGs in resources/svg.

For each square icon this renders the matching SVG group at every target size,
centers it on a transparent square canvas, losslessly optimizes the PNGs with
oxipng, then packs the .ico straight from the optimized PNG frames. The installer
side image is rendered aspect-preserved (it is a tall wizard banner, not square).

Layout under resources/images/ (nothing else is touched). Each multi-size icon
keeps its packed .ico in the root and its per-size PNGs in a subfolder named
after it; standalone images sit in the root:
  - <icon>.ico                  packed icon, root  (file-bas, file-bi, file-fbp,
                                                    file-fbs document types; fbide app)
  - <icon>/<size>.png           that icon's size variants, grouped per icon
  - installer.ico               setup.exe icon (SetupIconFile); no png set
  - installer-side.png          wizard side image (WizardImageFile), standalone
  - splash.png                  startup splash (shown at native size), standalone

Sizes: square icons carry 16/24/32/48/64/256 .ico frames; 128/512/1024 are
emitted as PNGs only (a Windows .ico tops out at 256) to cover the macOS .icns
set. Small-art frames (<=32) for bas/bi come from the *-small.svg groups; the
rest from the full-detail groups. The installer icon is .ico only (no .png set).

oxipng makes the PNGs (and the .ico frames packed from them) ~25-30% smaller,
fully lossless. Found on PATH, via OXIPNG / --oxipng, or a copy under build/oxipng/.
If none is found the icons are still written, just unoptimized.

The SVGs live in resources/svg. Override with --art or the FBIDE_ART_DIR env var.
Inkscape is found on PATH, via INKSCAPE, or at its default Windows install path.

Usage:
    python resources/scripts/make-doc-icons.py [--art <svg-dir>] [--inkscape <exe>] [--oxipng <exe>]

Requires: Inkscape 1.x, Pillow. Optional: oxipng (for optimal size).
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent.parent  # resources/scripts/ -> repo root
OUT_DIR = REPO_ROOT / "resources" / "images"  # every generated image lands here

SMALL_SIZES = [16, 24, 32]
LARGE_SIZES = [48, 64, 256]
ICO_SIZES = SMALL_SIZES + LARGE_SIZES  # frames packed into the .ico (<= 256)
# PNG-only sizes (no .ico frame): a Windows .ico tops out at 256, so the larger
# sizes are emitted as PNGs to cover the macOS .icns set (128/256/512 plus the
# @2x 1024). Rendered from full-detail art.
PNG_EXTRA_SIZES = [128, 512, 1024]

# Document icons: output name -> (small-art group id, large-art group id). The
# ids are the Inkscape group `id=` attributes (labels: file_source_bas etc).
# project/session have no dedicated *_small art, so both columns reuse the
# full-detail group.
DOC_ICONS = {
    "bas": ("g3", "g1"),
    "bi": ("g9", "g2"),
    "fbp": ("g4", "g4"),
    "fbs": ("g5-9", "g5"),
}

# Application icon: a single group in its own SVG.
APP_SVG = "fbide-icon.svg"
APP_GROUP = "g276"

# Installer (setup.exe) icon: single group, .ico only.
INSTALLER_SVG = "fbide-icon-install.svg"
INSTALLER_GROUP = "g285"

# Installer wizard side image: a tall banner. Rendered to Inno's exact modern
# wizard aspect (164:314) with aspect-fill — scaled to cover the frame plus a
# small overscan zoom, then center-cropped — so it fills cleanly with no
# letterbox / stretched edge. Height is ~3x the 314px logical size for HiDPI.
SIDE_SVG = "fbide-install.svg"
SIDE_GROUP = "g40883"
SIDE_RATIO = 164 / 314  # Inno modern WizardImageFile aspect
SIDE_HEIGHT = 942
SIDE_ZOOM = 1.03        # overscan: trims a few px off each edge

# Startup splash. Shown by wxSplashScreen at its native pixel size, so this
# height is the on-screen size.
SPLASH_SVG = "fbide-splash.svg"
SPLASH_GROUP = "g40479"
SPLASH_HEIGHT = 400


def find_tool(
    override: str | None,
    env_var: str,
    path_name: str,
    *,
    default: Path | None = None,
    search_dir: Path | None = None,
    exe_name: str | None = None,
) -> str | None:
    """Resolve a tool from an explicit override, env var, PATH, a fixed default,
    or a recursive search under search_dir (for a vendored copy)."""
    for cand in (override, os.environ.get(env_var)):
        if cand:
            if Path(cand).exists():
                return str(cand)
            on_path = shutil.which(cand)
            if on_path:
                return on_path
    on_path = shutil.which(path_name)
    if on_path:
        return on_path
    if default and default.exists():
        return str(default)
    if search_dir and search_dir.exists():
        hit = next(iter(sorted(search_dir.rglob(exe_name or path_name))), None)
        if hit:
            return str(hit)
    return None


def export_png(inkscape: str, svg: Path, group: str, height: int, dst: Path) -> None:
    """Render one group at `height` px tall (width proportional), transparent bg."""
    subprocess.run(
        [
            inkscape, str(svg),
            f"--export-id={group}", "--export-id-only",
            "--export-type=png", f"--export-filename={dst}",
            f"--export-height={height}",
        ],
        check=True, capture_output=True, text=True,
    )


def square_png(inkscape: str, svg: Path, group: str, size: int, tmp: Path, dst: Path) -> None:
    """Render the group, center it on a transparent size x size canvas, save PNG."""
    raw = tmp / f"{dst.stem}-raw.png"
    export_png(inkscape, svg, group, size, raw)
    with Image.open(raw) as src:
        img = src.convert("RGBA")
    if img.width > size or img.height > size:
        img.thumbnail((size, size), Image.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(img, ((size - img.width) // 2, (size - img.height) // 2))
    canvas.save(dst, format="PNG", optimize=True, compress_level=9)


def banner_png(
    inkscape: str, svg: Path, group: str, out_w: int, out_h: int, zoom: float, tmp: Path, dst: Path
) -> None:
    """Render a group and aspect-fill it into out_w x out_h: scale to cover the
    frame (times `zoom` overscan), then center-crop. No letterbox, no stretch."""
    raw = tmp / f"{dst.stem}-raw.png"
    export_png(inkscape, svg, group, out_h * 2, raw)  # oversample for a crisp downscale
    with Image.open(raw) as src:
        img = src.convert("RGBA")
    scale = max(out_w / img.width, out_h / img.height) * zoom
    img = img.resize((max(out_w, round(img.width * scale)), max(out_h, round(img.height * scale))), Image.LANCZOS)
    left = (img.width - out_w) // 2
    top = (img.height - out_h) // 2
    img = img.crop((left, top, left + out_w, top + out_h))
    img.save(dst, format="PNG", optimize=True, compress_level=9)


def flat_png(inkscape: str, svg: Path, group: str, height: int, tmp: Path, dst: Path) -> None:
    """Render a group at `height` px, native aspect, no crop. For full-bleed art."""
    raw = tmp / f"{dst.stem}-raw.png"
    export_png(inkscape, svg, group, height, raw)
    with Image.open(raw) as src:
        img = src.convert("RGBA")
    img.save(dst, format="PNG", optimize=True, compress_level=9)


def write_ico(frames: list[tuple[int, Path]], dst: Path) -> None:
    """Pack PNG frames (size, path) into an .ico, reusing their bytes verbatim."""
    items = sorted(frames, key=lambda t: t[0])
    blobs = [path.read_bytes() for _, path in items]
    header = struct.pack("<HHH", 0, 1, len(items))  # reserved, type=icon, count
    offset = len(header) + 16 * len(items)
    entries, data = b"", b""
    for (size, _), blob in zip(items, blobs):
        # A 0 width/height byte in an ICONDIRENTRY means 256.
        dim = 0 if size == 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
        data += blob
    dst.write_bytes(header + entries + data)


def optimize(oxipng: str | None, pngs: list[Path]) -> None:
    """Losslessly shrink every PNG in place (no-op without oxipng)."""
    if not oxipng:
        print("  oxipng not found - PNGs left unoptimized (pass --oxipng or add to PATH).")
        return
    # --alpha only rewrites fully-transparent pixels' RGB, so it stays lossless.
    subprocess.run(
        [oxipng, "-o", "max", "--strip", "safe", "--alpha", "-q", *map(str, pngs)],
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate FBIde app, document, and installer icons.")
    parser.add_argument(
        "--art",
        default=os.environ.get("FBIDE_ART_DIR", str(REPO_ROOT / "resources" / "svg")),
        help="Directory holding the icon SVGs (default: resources/svg).",
    )
    parser.add_argument("--inkscape", help="Path to the Inkscape executable.")
    parser.add_argument("--oxipng", help="Path to the oxipng executable (optional).")
    args = parser.parse_args()

    inkscape = find_tool(
        args.inkscape, "INKSCAPE", "inkscape",
        default=Path(r"C:\Program Files\Inkscape\bin\inkscape.exe"),
    )
    if not inkscape:
        sys.exit("Inkscape not found. Pass --inkscape <exe> or set INKSCAPE / PATH.")
    oxipng = find_tool(
        args.oxipng, "OXIPNG", "oxipng",
        search_dir=REPO_ROOT / "build" / "oxipng",
        exe_name="oxipng.exe" if os.name == "nt" else "oxipng",
    )

    art = Path(args.art)
    large_svg = art / "fbide-doc-icons.svg"
    small_svg = art / "fbide-doc-icons-small.svg"
    app_svg = art / APP_SVG
    installer_svg = art / INSTALLER_SVG
    side_svg = art / SIDE_SVG
    splash_svg = art / SPLASH_SVG
    for svg in (large_svg, small_svg, app_svg, installer_svg, side_svg, splash_svg):
        if not svg.exists():
            sys.exit(f"SVG not found: {svg}\nPass --art <dir> or set FBIDE_ART_DIR.")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    all_pngs: list[Path] = []
    ico_jobs: list[tuple[list[tuple[int, Path]], list[Path], str]] = []

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)

        # Document icons (small + large art) -> resources/images, .png set + 512.
        for name, (small_id, large_id) in DOC_ICONS.items():
            png_dir = OUT_DIR / f"file-{name}"
            png_dir.mkdir(exist_ok=True)
            frames = []
            for svg, gid, sizes in (
                (small_svg, small_id, SMALL_SIZES),
                (large_svg, large_id, LARGE_SIZES),
                (large_svg, large_id, PNG_EXTRA_SIZES),
            ):
                for size in sizes:
                    dst = png_dir / f"{size}.png"
                    square_png(inkscape, svg, gid, size, tmp, dst)
                    all_pngs.append(dst)
                    if size in ICO_SIZES:
                        frames.append((size, dst))
            ico_jobs.append((frames, [OUT_DIR / f"file-{name}.ico"], f"file-{name}"))

        # Application icon -> resources/images, .png set + 512.
        app_dir = OUT_DIR / "fbide"
        app_dir.mkdir(exist_ok=True)
        frames = []
        for size in ICO_SIZES + PNG_EXTRA_SIZES:
            dst = app_dir / f"{size}.png"
            square_png(inkscape, app_svg, APP_GROUP, size, tmp, dst)
            all_pngs.append(dst)
            if size in ICO_SIZES:
                frames.append((size, dst))
        ico_jobs.append((frames, [OUT_DIR / "fbide.ico"], "fbide"))

        # Installer (setup.exe) icon -> resources/images/installer.ico only. Frames
        # stay in tmp; no .png set is published for it.
        frames = []
        for size in ICO_SIZES:
            dst = tmp / f"installer-{size}.png"
            square_png(inkscape, installer_svg, INSTALLER_GROUP, size, tmp, dst)
            all_pngs.append(dst)
            frames.append((size, dst))
        ico_jobs.append((frames, [OUT_DIR / "installer.ico"], "installer"))

        # Installer wizard side image -> resources/images/installer-side.png (PNG,
        # aspect preserved; Inno 6.3+ accepts PNG for WizardImageFile).
        side = OUT_DIR / "installer-side.png"
        side_w = round(SIDE_HEIGHT * SIDE_RATIO)
        banner_png(inkscape, side_svg, SIDE_GROUP, side_w, SIDE_HEIGHT, SIDE_ZOOM, tmp, side)
        all_pngs.append(side)

        # Startup splash -> resources/images (full-bleed, native aspect).
        splash = OUT_DIR / "splash.png"
        flat_png(inkscape, splash_svg, SPLASH_GROUP, SPLASH_HEIGHT, tmp, splash)
        all_pngs.append(splash)

        optimize(oxipng, all_pngs)

        for frames, destinations, label in ico_jobs:
            for dst in destinations:
                write_ico(frames, dst)
            sizes_str = ", ".join(str(s) for s, _ in sorted(frames))
            png_note = f" + {len(frames) + len(PNG_EXTRA_SIZES)} png" if label != "installer" else ""
            print(f"  {label}: ico [{sizes_str}]{png_note}")
        with Image.open(side) as im:
            print(f"  installer-side.png: {im.width}x{im.height}")
        with Image.open(splash) as im:
            print(f"  splash.png: {im.width}x{im.height}")

    print(f"Wrote icons to {OUT_DIR.relative_to(REPO_ROOT)}"
          + (" (oxipng-optimized)" if oxipng else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
