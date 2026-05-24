# Changes since 0.5.0-rc.2

- Added change tracking (#1)
- Added a confirm-and-close prompt when Save As targets a file already open in another tab
- Added basic Markdown syntax highlighting for `.md` / `.markdown` files
- Added basic Windows batch syntax highlighting for `.bat` / `.cmd` files
- Added basic shell script syntax highlighting for `.sh` / `.bash` files
- Added basic Makefile syntax highlighting (`Makefile`, `GNUmakefile`, `.mk`, `.make`)
- Added basic JSON syntax highlighting for `.json` / `.json5` files
- Added basic CSS syntax highlighting for `.css` files
- Added a document type indicator to the status bar
- Added option to override document type (right click on type indicator in the status bar)
- Changed fbide now stores config changes in `.local.`, keeping original base files immutable.
- Fixed `#include` opening the same file in multiple tabs on case-insensitive filesystems (#87)
- Fixed issue with folding where the opener was followed by empty lines.

# Changes since 0.5.0-rc.1

- Added editor minimap — toggle via View → Minimap (F6)
- Fixed perfomance issues with multiline edits (undo/redo/index, etc.)
- Fixed unsaved FBIDETEMP location to be fbide cwd (#70)
- Fixed "Cmd prompt" command, which messed with fbide cwd
- Fixed symbol browser not listing Sub/Function declarations with `Private`/`Public`/`Protected` modifiers (#74)
- Changed symbol browser to show the qualified name for methods (`Type.Method`)
- Added constructors, destructors, operators and properties to the symbol browser
- Changed symbol browser to group UDT methods (subs, functions, constructors, …) under their owning type
- Added a search box to the symbol browser to filter results live by name, symbol type or UDT
- Added auto-indent and closer insertion for preprocessor blocks (#72)
- Changed symbol browser to list symbols declared inside `#if`/`#ifdef`/`#ifndef` blocks (#73)
- Fixed several issues around folding (#55)

# Changes since 0.5.0-beta.3

- Added new cobalt theme (Gothon)
- New styles for preprocessor identifier, number, operator and strings
- Changed theme settings to use a tree layout for an easier overview
- Fixed wstring and zstring classification, moved them to type keywords (#47)
- Fixed detection when a compound statement closer is not needed (#50)
- Fixed avoid de-indenting compound statement closers beyond opener level (#48)
- Fixed single-line `asm` statements no longer treated as multi-line block openers (#46)
- Fixed auto-indent and code formatter no longer add `End Asm` closer / indent body for single-line `asm` (#46)
- Fixed preprocessor lexing of `"_"` which would cause `_` to be treated as a line continuation (#54)
- Changed IDE resource directory carrying a `READONLY` sentinel file is now mirrored to `<user-data-dir>/ide` on launch and loaded/saved from there. (#39)
- Added Linux AppImage to the release pipeline; `-DBUILD_APPIMAGE=ON` switches the install layout to FHS and points the resource resolver at `share/fbide/ide/` (#66)
- Changed log now writes to the per-user data directory by default and flushes after each record, so crash diagnostics survive; added `--log-path` to override the location
- Changed `<$terminal>` placeholder is now read from `compiler.terminal` config(#45)
- Added run and compile command placeholder table.

# Changes since 0.5.0-beta.2

- Fixed issue with auto-indent not working when keyword case transform was disabled (#24)
- Fixed issue with REM comment followed by newline, which caused next line to be a comment (#28)
- Fixed various spelling errors (#26)
- Fixed bug with console not being properly updated after compile-generated output (#32)
- Fixed bug with indenting #defines containing IF keyword, which was counted as ppIF (#34)
- Fixed toolbar tooltip to show long help string (#36)
- Fixed case transform behaviour when moving cursor
- Changed auto indenter actions are now undoable (#37)
- Added show current document path in the title bar (#38)
- Renamed "Sub/Function browser" to "Symbol browser" across all locales
- Added Spanish translation (Joseba Epalza)

# Change since 0.5.0-beta.1

- Improved dark mode support on Windows. Can be enabled manually by editing the config file
- Toolbar now uses a new method to render, allowing it to be moved around, docked to the sides, etc. (#11)
- Remember the configured layout between FBIDE restarts
- Encode correct platform values in Windows manifest files
- The font now falls back to the system default monospace when the configured face isn't installed (#12)
- Pressing `F2` now toggles the Browser pane; `Shift+F2` opens the Sub/Function browser (#9)
- Removed old version from splash image (pauldoe)
- Session files are now persisted in the "Recent Files" menu, allowing quick access. (#16)
- Sessions now persist code folding state (#10)
- Fix issue with compile log not populating properly (#6, #7)
- Right-clicking on the console now opens the compile log (#5)
- Fixed issue with default background colour for margins, and fixed classic theme (#4)
- Support drag & drop files, only files supported by fbide are opened (#21)

# Changes since fbide 0.4.5.

### Project changes
- FBIde is completely rewritten from scratch, to match the feature set of fbide 0.4.6.
- Updated to wxWidgets 3.3.
- Adopted C++23 as baseline.
- Adopted the CMake build system.
- Relicensed under MIT

### General
- Added option to start the editor with a custom config file via `--config` option, or setting ide path with `--ide`
- Line numbers margin now zooms with font size changes
- FBIde validates chm files, if they are locked, show a dialogue with instructions and a link for more info
- If no local CHM file is found, or when on a non-Windows platform, open the FreeBASIC wiki page instead
- Added More keyword groups in settings
- Added ASM syntax highlighting
- Simplified Theme settings
- Added config file for shortcuts
- Added Config file for menu and toolbar layouts
- Added useful links in the Help menu
- Added support for INI (and other fbide config files) files
- Added support for different file encodings and line endings
- Updated session files to store some file properties like encoding and line endings
- Changed session to store relative paths when possible
- Added support for case transform of keywords
- Improved auto-indent by automatically adding closing keywords. After DO, add LOOP automatically.
- Reorganised keyword groups.
- Sub/Function browser now also shows included files, clicking on which opens the file
- Sub/Function browser now also shows macros
- Added dropdown menu option for reloading file from disk

### Removed options
- `ActivePath` option has been removed. FBIde now always sets the working directory to the source file location when compiling or running.
- `floatbars` option has been removed. FBIde now always uses wxAUI for layout.

### Layout
- FBIde uses wxAUI layout for tabs and panels. They can be moved around, docked in various positions, etc.

### Compiler
- Compilation is now asynchronous in a separate process.
- Compiler log is now a single persistent window that updates automatically after each compilation.
- Compile command paths are quoted by default: `"<$fbc>" "<$file>"`.
- Run command paths are quoted by default: `<$terminal> "<$file>" <$param>`.
- Added toolbar button to kill running process
