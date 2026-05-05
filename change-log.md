# Change since 0.5.0-beta.2

- Fixed issue with auto indent not working when keyword case transform was disabled (#24)
- Fixed issue with REM comment followed by newline, which caused next line to be a comment (#28)
- Fixed various spelling errors (#26)
- Fixed bug with console not being properly updated after compile generated output (#32)
- Fixed bug with indenting #defines containing IF keyword, which was counted as ppIF (#34)
- Fixed toolbar tooltip to shohw long help string (#36)

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
