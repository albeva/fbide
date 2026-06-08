# FBIde

**An open-source IDE for the [FreeBASIC](https://freebasic.net) compiler.**

FBIde is a lightweight, cross-platform editor and build environment for the
FreeBASIC programming language. It pairs a fast, syntax-aware editor with
one-key compilation and execution, so you can go from source to running program
without leaving the window.

```freebasic
' A small FreeBASIC program
#include "fbgfx.bi"

dim as integer count = 3
for i as integer = 1 to count
    print "Hello, World! "; i
next
```

## Features

- FreeBASIC syntax highlighting and brace matching
- Indentation guides and automatic indentation
- Integrated compilation and execution
- Tabbed editing with session save / restore
- Customizable themes and editor settings
- Multilingual interface

## Links

- **Website** — <https://fbide.freebasic.net>
- **Repository** — <https://github.com/albeva/fbide>
- **FreeBASIC** — <https://freebasic.net>

## License

FBIde is released under the **MIT License**. See the bundled `LICENSE` file for
details, and `THIRD_PARTY_LICENSES.txt` for the licenses of bundled components.

Copyright © 2006–2026 Albert Varaksin.

## Credits

- **paul doe** — cleaned up the original splash image and made it re-usable
- **Gothon** — created the Cobalt theme
- **dumbledore** — code exporting and formatting routines (legacy FBIde)
- **Madedog** — internationalization modules (legacy FBIde)
- **Mecki** — splash screen logo (legacy FBIde)
- **MySoft** — many patches and fixes

With thanks to the FBIde and FreeBASIC beta testers and community.
