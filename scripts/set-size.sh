#!/bin/bash
# Resize the front FBIde window. Defaults: 800×600 at (100, 100).
#
# Usage:
#   fbide-size                   # 800×600 at (100,100)
#   fbide-size 1024 768          # 1024×768 at (100,100)
#   fbide-size 1024 768 50 50    # 1024×768 at (50,50)
set -euo pipefail

W="${1:-800}"
H="${2:-600}"
X="${3:-100}"
Y="${4:-100}"

osascript <<eof
tell application "System Events"
    tell process "FBIde"
        set frontmost TO true
        set position of front window TO {${X}, ${Y}}
        set size of front window TO {${W}, ${H}}
    END tell
END tell
eof
