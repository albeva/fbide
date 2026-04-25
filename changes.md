Changes from fbide 0.4.5.

## Project changes
- FBIde is completely rewritten from scratch, to match feature set of fbide 0.4.6.
- Updated to wxWidgets 3.3.
- Adopted C++23 as baseline.
- Adopted cmake build system.
- Relicenced under MIT

## General
- Added option to start the editor with a custom config file via `--config` option
- Line numbers margin now zooms with font size changes
- FBIde validates chm files, if they are locked, show a dialog with instructions and a link for more info
- If no local CHM file found, or when on non Windows platform, open freebasic wiki page instead
- Added More keyword groups in settings
- Added ASM syntax highlighting
- Simplified Theme settings
- Added config file for shortcuts
- Added Config file for menu and toolbar layouts
- Added useful linkes in the Help menu
- Added support for INI (and other fbide config files) files
- Added support for different file encodings and line endings
- Updated session files to store some file properties like encoding and line endings
- Chnaged session to store relative paths when possible
- Added support for case transform of keywords
- Improved auto indent by automatically adding closer keywords. after DO, add LOOP automatically.
- Reorganized keyword groups.

## Removed options
- `ActivePath` option has been removed. FBIde now always sets the working directory to the source file location when compiling or running.
- `floatbars` option has been removed. FBIde now always uses wxAUI for layout.

## Layout
- FBIde uses wxAUI layout for tabs and panels. They can be moved around, docked in various positions, etc.

## Compiler
- Compilation is now asynchronous in a separate process.
- Compiler log is now a single persistent window that updates automatically after each compilation.
- Compile command paths are quoted by default: `"<$fbc>" "<$file>"`.
- Run command paths are quoted by default: `<$terminal> "<$file>" <$param>`.
- Added toolbar button to kill running process
