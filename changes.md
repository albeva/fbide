Changes from fbide 0.4.5.

## Project changes
- FBIde is completely rewritten from scratch, to match feature set of fbide 0.4.6.
- Updated to wxWidgets 3.3.
- Adopted C++23 as baseline.
- Adopted cmake build system.

## General
- Added support for startign the editor with custom config file via `--config` command line option.

## Removed options
- `ActivePath` option has been removed. FBIde now always sets the working directory to the source file location when compiling or running.
- `floatbars` option has been removed. FBIde now always uses wxAUI for layout.

## Layout
- FBIde uses wxAUI layout for tabs and panels. They can be moved around, docked in various positions, etc.

## Compiler
- Compilation is now asynchronous. The UI remains responsive while the compiler runs.
- Compiler log is now a single persistent window that updates automatically after each compilation.
- Compile command paths are quoted by default: `"<$fbc>" "<$file>"`.
- Run command paths are quoted by default: `<$terminal> "<$file>" <$param>`.
