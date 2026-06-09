#!/usr/bin/env python3
"""Compare locale .ini files against en.ini and report missing / extra keys.

en.ini is the source of truth: every other locale must define every key it
has. Keys are identified by their section + name (e.g. "[dialogs/log] title"),
so the same name under two sections counts as two distinct keys.

Usage:
    python resources/scripts/check_locales.py                # check every locale
    python resources/scripts/check_locales.py fr de          # check only fr, de
    python resources/scripts/check_locales.py --extra        # also report extra keys
    python resources/scripts/check_locales.py --dir path/to/locales

Exit code is non-zero when any checked locale is missing keys, so this can
gate CI.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

DEFAULT_DIR = Path(__file__).resolve().parent.parent / "ide" / "locales"
REFERENCE = "en"


def parse_keys(path: Path) -> dict[str, int]:
    """Return {("[section] key"): line_number} for one .ini file.

    Comments (`;`) and blank lines are skipped. Section headers `[name]`
    set the current section; `key=value` lines record the key.
    """
    keys: dict[str, int] = {}
    section = ""
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            continue
        if "=" in line:
            name = line.split("=", 1)[0].strip()
            keys[f"[{section}] {name}"] = lineno
    return keys


def main() -> int:
    ap = argparse.ArgumentParser(description="Compare locale files to en.ini.")
    ap.add_argument("locales", nargs="*", help="locale codes to check (default: all)")
    ap.add_argument("--dir", type=Path, default=DEFAULT_DIR, help="locales directory")
    ap.add_argument("--extra", action="store_true", help="also report keys not in en.ini")
    args = ap.parse_args()

    ref_path = args.dir / f"{REFERENCE}.ini"
    if not ref_path.is_file():
        print(f"reference not found: {ref_path}", file=sys.stderr)
        return 2
    ref_keys = parse_keys(ref_path)

    if args.locales:
        targets = [args.dir / f"{code}.ini" for code in args.locales]
    else:
        targets = sorted(p for p in args.dir.glob("*.ini") if p.stem != REFERENCE)

    total_missing = 0
    for path in targets:
        if not path.is_file():
            print(f"  ! {path.name}: file not found")
            total_missing += 1
            continue
        keys = parse_keys(path)
        missing = sorted(set(ref_keys) - set(keys))
        extra = sorted(set(keys) - set(ref_keys)) if args.extra else []
        if not missing and not extra:
            print(f"  OK {path.stem}  ({len(keys)} keys)")
            continue
        summary = f"  -- {path.stem}: {len(missing)} missing"
        if args.extra:
            summary += f", {len(extra)} extra"
        print(summary)
        for k in missing:
            print(f"       MISSING {k}")
        for k in extra:
            print(f"       EXTRA   {k}")
        total_missing += len(missing)

    print()
    if total_missing:
        print(f"FAIL: {total_missing} missing key(s) across checked locales")
        return 1
    print("OK: every checked locale has all en.ini keys")
    return 0


if __name__ == "__main__":
    sys.exit(main())
